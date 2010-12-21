/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is JavaScript shell workers.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jason Orendorff <jorendorff@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifdef JS_THREADSAFE

#include <algorithm>
#include <string.h>
#include "prthread.h"
#include "prlock.h"
#include "prcvar.h"
#include "jsapi.h"
#include "jscntxt.h"
#include "jshashtable.h"
#include "jsstdint.h"
#include "jslock.h"
#include "jsvector.h"
#include "jsworkers.h"

extern size_t gMaxStackSize;

/*
 * JavaScript shell workers.
 *
 * == Object lifetime rules ==
 *
 *   - The ThreadPool lasts from init() to finish().
 *
 *   - The ThreadPool owns the MainQueue and the WorkerQueue. Those live from
 *     the time the first Worker is created until finish().
 *
 *   - Each JS Worker object has the same lifetime as the corresponding C++
 *     Worker object. A Worker is live if (a) the Worker JSObject is still
 *     live; (b) the Worker has an incoming event pending or running; (c) it
 *     has sent an outgoing event to its parent that is still pending; or (d)
 *     it has any live child Workers.
 *
 *   - finish() continues to wait for events until all threads are idle.
 *
 * Event objects, however, are basically C++-only. The JS Event objects are
 * just plain old JSObjects. They don't keep anything alive.
 *
 * == Locking scheme ==
 *
 * When mixing mutexes and the JSAPI request model, there are two choices:
 *
 *   - Always nest the mutexes in requests. Since threads in requests are not
 *     supposed to block, this means the mutexes must be only briefly held.
 *
 *   - Never nest the mutexes in requests. Since this allows threads to race
 *     with the GC, trace() methods must go through the mutexes just like
 *     everyone else.
 *
 * This code uses the latter approach for all locks.
 *
 * In one case, a thread holding a Worker's mutex can acquire the mutex of one
 * of its child Workers. See Worker::terminateSelf. (This can't deadlock because
 * the parent-child relationship is a partial order.)
 */

namespace js {
namespace workers {

template <class T, class AllocPolicy>
class Queue {
  private:
    typedef Vector<T, 4, AllocPolicy> Vec;
    Vec v1;
    Vec v2;
    Vec *front;
    Vec *back;

    // Queue is not copyable.
    Queue(const Queue &);
    Queue & operator=(const Queue &);

  public:
    Queue() : front(&v1), back(&v2) {}
    bool push(T t) { return back->append(t); }
    bool empty() { return front->empty() && back->empty(); }

    T pop() {
        if (front->empty()) {
            std::reverse(back->begin(), back->end());
            Vec *tmp = front;
            front = back;
            back = tmp;
        }
        T item = front->back();
        front->popBack();
        return item;
    }        

    void clear() {
        v1.clear();
        v2.clear();
    }

    void trace(JSTracer *trc) {
        for (T *p = v1.begin(); p != v1.end(); p++)
            (*p)->trace(trc);
        for (T *p = v2.begin(); p != v2.end(); p++)
            (*p)->trace(trc);
    }
};

class Event;
class ThreadPool;
class Worker;

class WorkerParent {
  protected:
    typedef HashSet<Worker *, DefaultHasher<Worker *>, SystemAllocPolicy> ChildSet;
    ChildSet children;

    bool initWorkerParent() { return children.init(8); }

  public:
    virtual JSLock *getLock() = 0;
    virtual ThreadPool *getThreadPool() = 0;
    virtual bool post(Event *item) = 0;  // false on OOM or queue closed
    virtual void trace(JSTracer *trc) = 0;

    bool addChild(Worker *w) {
        AutoLock hold(getLock());
        return children.put(w) != NULL;
    }

    // This must be called only from GC or when all threads are shut down. It
    // does not bother with locking.
    void removeChild(Worker *w) {
        ChildSet::Ptr p = children.lookup(w);
        JS_ASSERT(p);
        children.remove(p);
    }

    void disposeChildren();
};

template <class T>
class ThreadSafeQueue
{
  protected:
    Queue<T, SystemAllocPolicy> queue;
    JSLock *lock;
    PRCondVar *condvar;
    bool closed;

  private:
    Vector<T, 8, SystemAllocPolicy> busy;

  protected:
    ThreadSafeQueue() : lock(NULL), condvar(NULL), closed(false) {}

    ~ThreadSafeQueue() {
        if (condvar)
            JS_DESTROY_CONDVAR(condvar);
        if (lock)
            JS_DESTROY_LOCK(lock);
    }

    // Called by take() with the lock held.
    virtual bool shouldStop() { return closed; }

  public:
    bool initThreadSafeQueue() {
        JS_ASSERT(!lock);
        JS_ASSERT(!condvar);
        return (lock = JS_NEW_LOCK()) && (condvar = JS_NEW_CONDVAR(lock));
    }

    bool post(T t) {
        AutoLock hold(lock);
        if (closed)
            return false;
        if (queue.empty())
            JS_NOTIFY_ALL_CONDVAR(condvar);
        return queue.push(t);
    }

    void close() {
        AutoLock hold(lock);
        closed = true;
        queue.clear();
        JS_NOTIFY_ALL_CONDVAR(condvar);
    }

    // The caller must hold the lock.
    bool take(T *t) {
        while (queue.empty()) {
            if (shouldStop())
                return false;
            JS_WAIT_CONDVAR(condvar, JS_NO_TIMEOUT);
        }
        *t = queue.pop();
        busy.append(*t);
        return true;
    }

    // The caller must hold the lock.
    void drop(T item) {
        for (T *p = busy.begin(); p != busy.end(); p++) {
            if (*p == item) {
                *p = busy.back();
                busy.popBack();
                return;
            }
        }
        JS_NOT_REACHED("removeBusy");
    }

    bool lockedIsIdle() { return busy.empty() && queue.empty(); }

    bool isIdle() {
        AutoLock hold(lock);
        return lockedIsIdle();
    }

    void wake() {
        AutoLock hold(lock);
        JS_NOTIFY_ALL_CONDVAR(condvar);
    }

    void trace(JSTracer *trc) {
        AutoLock hold(lock);
        for (T *p = busy.begin(); p != busy.end(); p++)
            (*p)->trace(trc);
        queue.trace(trc);
    }
};

class MainQueue;

class Event
{
  protected:
    virtual ~Event() { JS_ASSERT(!data); }

    WorkerParent *recipient;
    Worker *child;
    uint64 *data;
    size_t nbytes;

  public:
    enum Result { fail = JS_FALSE, ok = JS_TRUE, forwardToParent };

    virtual void destroy(JSContext *cx) { 
        JS_free(cx, data);
#ifdef DEBUG
        data = NULL;
#endif
        delete this;
    }

    void setChildAndRecipient(Worker *aChild, WorkerParent *aRecipient) {
        child = aChild;
        recipient = aRecipient;
    }

    bool deserializeData(JSContext *cx, jsval *vp) {
        return !!JS_ReadStructuredClone(cx, data, nbytes, JS_STRUCTURED_CLONE_VERSION, vp);
    }

    virtual Result process(JSContext *cx) = 0;

    inline void trace(JSTracer *trc);

    template <class EventType>
    static EventType *createEvent(JSContext *cx, WorkerParent *recipient, Worker *child,
                                  jsval v)
    {
        uint64 *data;
        size_t nbytes;
        if (!JS_WriteStructuredClone(cx, v, &data, &nbytes))
            return NULL;

        EventType *event = new EventType;
        if (!event) {
            JS_ReportOutOfMemory(cx);
            return NULL;
        }
        event->recipient = recipient;
        event->child = child;
        event->data = data;
        event->nbytes = nbytes;
        return event;
    }

    Result dispatch(JSContext *cx, JSObject *thisobj, const char *dataPropName,
                    const char *methodName, Result noHandler)
    {
        if (!data)
            return fail;

        JSBool found;
        if (!JS_HasProperty(cx, thisobj, methodName, &found))
            return fail;
        if (!found)
            return noHandler;

        // Create event object.
        jsval v;
        if (!deserializeData(cx, &v))
            return fail;
        JSObject *obj = JS_NewObject(cx, NULL, NULL, NULL);
        if (!obj || !JS_DefineProperty(cx, obj, dataPropName, v, NULL, NULL, 0))
            return fail;

        // Call event handler.
        jsval argv[1] = { OBJECT_TO_JSVAL(obj) };
        jsval rval = JSVAL_VOID;
        return Result(JS_CallFunctionName(cx, thisobj, methodName, 1, argv, &rval));
    }
};

typedef ThreadSafeQueue<Event *> EventQueue;

class MainQueue : public EventQueue, public WorkerParent
{
  private:
    ThreadPool *threadPool;

  public:
    explicit MainQueue(ThreadPool *tp) : threadPool(tp) {}

    ~MainQueue() {
        JS_ASSERT(queue.empty());
    }

    bool init() { return initThreadSafeQueue() && initWorkerParent(); }

    void destroy(JSContext *cx) {
        while (!queue.empty())
            queue.pop()->destroy(cx);
        delete this;
    }

    virtual JSLock *getLock() { return lock; }
    virtual ThreadPool *getThreadPool() { return threadPool; }

  protected:
    virtual bool shouldStop();

  public:
    virtual bool post(Event *event) { return EventQueue::post(event); }

    virtual void trace(JSTracer *trc);

    void traceChildren(JSTracer *trc) { EventQueue::trace(trc); }

    JSBool mainThreadWork(JSContext *cx, bool continueOnError) {
        JSAutoSuspendRequest suspend(cx);
        AutoLock hold(lock);

        Event *event;
        while (take(&event)) {
            JS_RELEASE_LOCK(lock);
            Event::Result result;
            {
                JSAutoRequest req(cx);
                result = event->process(cx);
                if (result == Event::forwardToParent) {
                    // FIXME - pointlessly truncates the string to 8 bits
                    jsval data;
                    JSAutoByteString bytes;
                    if (event->deserializeData(cx, &data) &&
                        JSVAL_IS_STRING(data) &&
                        bytes.encode(cx, JSVAL_TO_STRING(data))) {
                        JS_ReportError(cx, "%s", bytes.ptr());
                    } else {
                        JS_ReportOutOfMemory(cx);
                    }
                    result = Event::fail;
                }
                if (result == Event::fail && continueOnError) {
                    if (JS_IsExceptionPending(cx) && !JS_ReportPendingException(cx))
                        JS_ClearPendingException(cx);
                    result = Event::ok;
                }
            }
            JS_ACQUIRE_LOCK(lock);
            drop(event);
            event->destroy(cx);
            if (result != Event::ok)
                return false;
        }
        return true;
    }
};

/*
 * A queue of workers.
 *
 * We keep a queue of workers with pending events, rather than a queue of
 * events, so that two threads won't try to run a Worker at the same time.
 */
class WorkerQueue : public ThreadSafeQueue<Worker *>
{
  private:
    MainQueue *main;

  public:
    explicit WorkerQueue(MainQueue *main) : main(main) {}

    void work();
};

/* The top-level object that owns everything else. */
class ThreadPool
{
  private:
    enum { threadCount = 6 };

    JSObject *obj;
    WorkerHooks *hooks;
    MainQueue *mq;
    WorkerQueue *wq;
    PRThread *threads[threadCount];
    int32_t terminating;

    static JSClass jsClass;

    static void start(void* arg) {
        ((WorkerQueue *) arg)->work();
    }

    explicit ThreadPool(WorkerHooks *hooks) : hooks(hooks), mq(NULL), wq(NULL), terminating(0) {
        for (int i = 0; i < threadCount; i++)
            threads[i] = NULL;
    }

  public:
    ~ThreadPool() {
        JS_ASSERT(!mq);
        JS_ASSERT(!wq);
        JS_ASSERT(!threads[0]);
    }

    static ThreadPool *create(JSContext *cx, WorkerHooks *hooks) {
        ThreadPool *tp = new ThreadPool(hooks);
        if (!tp) {
            JS_ReportOutOfMemory(cx);
            return NULL;
        }

        JSObject *obj = JS_NewObject(cx, &jsClass, NULL, NULL);
        if (!obj || !JS_SetPrivate(cx, obj, tp)) {
            delete tp;
            return NULL;
        }
        tp->obj = obj;
        return tp;
    }

    JSObject *asObject() { return obj; }
    WorkerHooks *getHooks() { return hooks; }
    WorkerQueue *getWorkerQueue() { return wq; }
    MainQueue *getMainQueue() { return mq; }
    bool isTerminating() { return terminating != 0; }

    /*
     * Main thread only. Requires request (to prevent GC, which could see the
     * object in an inconsistent state).
     */
    bool start(JSContext *cx) {
        JS_ASSERT(!mq && !wq);
        mq = new MainQueue(this);
        if (!mq || !mq->init()) {
            mq->destroy(cx);
            mq = NULL;
            return false;
        }
        wq = new WorkerQueue(mq);
        if (!wq || !wq->initThreadSafeQueue()) {
            delete wq;
            wq = NULL;
            mq->destroy(cx);
            mq = NULL;
            return false;
        }
        JSAutoSuspendRequest suspend(cx);
        bool ok = true;
        for (int i = 0; i < threadCount; i++) {
            threads[i] = PR_CreateThread(PR_USER_THREAD, start, wq, PR_PRIORITY_NORMAL,
                                         PR_LOCAL_THREAD, PR_JOINABLE_THREAD, 0);
            if (!threads[i]) {
                shutdown(cx);
                ok = false;
                break;
            }
        }
        return ok;
    }

    void terminateAll(JSRuntime *rt) {
        // See comment about JS_ATOMIC_SET in the implementation of
        // JS_TriggerOperationCallback.
        JS_ATOMIC_SET(&terminating, 1);
        JS_TriggerAllOperationCallbacks(rt);
    }

    /* This context is used only to free memory. */
    void shutdown(JSContext *cx) {
        wq->close();
        for (int i = 0; i < threadCount; i++) {
            if (threads[i]) {
                PR_JoinThread(threads[i]);
                threads[i] = NULL;
            }
        }

        delete wq;
        wq = NULL;

        mq->disposeChildren();
        mq->destroy(cx);
        mq = NULL;
        terminating = 0;
    }

  private:
    static void jsTraceThreadPool(JSTracer *trc, JSObject *obj) {
        ThreadPool *tp = unwrap(trc->context, obj);
        if (tp->mq) {
            tp->mq->traceChildren(trc);
            tp->wq->trace(trc);
        }
    }


    static void jsFinalize(JSContext *cx, JSObject *obj) {
        if (ThreadPool *tp = unwrap(cx, obj))
            delete tp;
    }

  public:
    static ThreadPool *unwrap(JSContext *cx, JSObject *obj) {
        JS_ASSERT(JS_GET_CLASS(cx, obj) == &jsClass);
        return (ThreadPool *) JS_GetPrivate(cx, obj);
    }
};

/*
 * A Worker is always in one of 4 states, except when it is being initialized
 * or destroyed, or its lock is held:
 *   - idle       (!terminated && current == NULL && events.empty())
 *   - enqueued   (!terminated && current == NULL && !events.empty())
 *   - busy       (!terminated && current != NULL)
 *   - terminated (terminated && current == NULL && events.empty())
 *
 * Separately, there is a terminateFlag that other threads can set
 * asynchronously to tell the Worker to terminate.
 */
class Worker : public WorkerParent
{
  private:
    ThreadPool *threadPool;
    WorkerParent *parent;
    JSObject *object;  // Worker object exposed to parent
    JSContext *context;
    JSLock *lock;
    Queue<Event *, SystemAllocPolicy> events;  // owning pointers to pending events
    Event *current;
    bool terminated;
    int32_t terminateFlag;

    static JSClass jsWorkerClass;

    Worker()
        : threadPool(NULL), parent(NULL), object(NULL),
          context(NULL), lock(NULL), current(NULL), terminated(false), terminateFlag(0) {}

    bool init(JSContext *parentcx, WorkerParent *parent, JSObject *obj) {
        JS_ASSERT(!threadPool && !this->parent && !object && !lock);

        if (!initWorkerParent() || !parent->addChild(this))
            return false;
        threadPool = parent->getThreadPool();
        this->parent = parent;
        this->object = obj;
        lock = JS_NEW_LOCK();
        return lock &&
               createContext(parentcx, parent) &&
               JS_SetPrivate(parentcx, obj, this);
    }

    bool createContext(JSContext *parentcx, WorkerParent *parent) {
        JSRuntime *rt = JS_GetRuntime(parentcx);
        context = JS_NewContext(rt, 8192);
        if (!context)
            return false;

        // The Worker has a strong reference to the global; see jsTraceWorker.
        // JSOPTION_UNROOTED_GLOBAL ensures that when the worker becomes
        // unreachable, it and its global object can be collected. Otherwise
        // the cx->globalObject root would keep them both alive forever.
        JS_SetOptions(context, JS_GetOptions(parentcx) | JSOPTION_UNROOTED_GLOBAL |
                                                         JSOPTION_DONT_REPORT_UNCAUGHT);
        JS_SetVersion(context, JS_GetVersion(parentcx));
        JS_SetContextPrivate(context, this);
        JS_SetOperationCallback(context, jsOperationCallback);
        JS_BeginRequest(context);

        JSObject *global = threadPool->getHooks()->newGlobalObject(context);
        JSObject *post, *proto, *ctor;
        if (!global)
            goto bad;
        JS_SetGlobalObject(context, global);

        // Because the Worker is completely isolated from the rest of the
        // runtime, and because any pending events on a Worker keep the Worker
        // alive, this postMessage function cannot be called after the Worker
        // is collected.  Therefore it's safe to stash a pointer (a weak
        // reference) to the C++ Worker object in the reserved slot.
        post = JS_GetFunctionObject(JS_DefineFunction(context, global, "postMessage",
                                                      (JSNative) jsPostMessageToParent, 1, 0));
        if (!post || !JS_SetReservedSlot(context, post, 0, PRIVATE_TO_JSVAL(this)))
            goto bad;

        proto = JS_InitClass(context, global, NULL, &jsWorkerClass, jsConstruct, 1,
                             NULL, jsMethods, NULL, NULL);
        if (!proto)
            goto bad;

        ctor = JS_GetConstructor(context, proto);
        if (!ctor || !JS_SetReservedSlot(context, ctor, 0, PRIVATE_TO_JSVAL(this)))
            goto bad;

        JS_EndRequest(context);
        JS_ClearContextThread(context);
        return true;

    bad:
        JS_EndRequest(context);
        JS_DestroyContext(context);
        context = NULL;
        return false;
    }

    static void jsTraceWorker(JSTracer *trc, JSObject *obj) {
        JS_ASSERT(JS_GET_CLASS(trc->context, obj) == &jsWorkerClass);
        if (Worker *w = (Worker *) JS_GetPrivate(trc->context, obj)) {
            w->parent->trace(trc);
            w->events.trace(trc);
            if (w->current)
                w->current->trace(trc);
            JS_CALL_OBJECT_TRACER(trc, JS_GetGlobalObject(w->context), "Worker global");
        }
    }

    static void jsFinalize(JSContext *cx, JSObject *obj) {
        JS_ASSERT(JS_GET_CLASS(cx, obj) == &jsWorkerClass);
        if (Worker *w = (Worker *) JS_GetPrivate(cx, obj))
            delete w;
    }

    static JSBool jsOperationCallback(JSContext *cx) {
        Worker *w = (Worker *) JS_GetContextPrivate(cx);
        JSAutoSuspendRequest suspend(cx);  // avoid nesting w->lock in a request
        return !w->checkTermination();
    }

    static JSBool jsResolveGlobal(JSContext *cx, JSObject *obj, jsid id, uintN flags,
                                  JSObject **objp)
    {
        JSBool resolved;

        if (!JS_ResolveStandardClass(cx, obj, id, &resolved))
            return false;
        if (resolved)
            *objp = obj;

        return true;
    }

    static JSBool jsPostMessageToParent(JSContext *cx, uintN argc, jsval *vp);
    static JSBool jsPostMessageToChild(JSContext *cx, uintN argc, jsval *vp);
    static JSBool jsTerminate(JSContext *cx, uintN argc, jsval *vp);

    bool checkTermination() {
        AutoLock hold(lock);
        return lockedCheckTermination();
    }

    bool lockedCheckTermination() {
        if (terminateFlag || threadPool->isTerminating()) {
            terminateSelf();
            terminateFlag = 0;
        }
        return terminated;
    }

    // Caller must hold the lock.
    void terminateSelf() {
        terminated = true;
        while (!events.empty())
            events.pop()->destroy(context);

        // Tell the children to shut down too. An arbitrarily silly amount of
        // processing could happen before the whole tree is terminated; but
        // this way we don't have to worry about blowing the C stack.
        for (ChildSet::Enum e(children); !e.empty(); e.popFront())
            e.front()->setTerminateFlag();  // note: nesting locks here
    }

  public:
    ~Worker() {
        if (parent)
            parent->removeChild(this);
        dispose();
    }

    void dispose() {
        JS_ASSERT(!current);
        while (!events.empty())
            events.pop()->destroy(context);
        if (lock) {
            JS_DESTROY_LOCK(lock);
            lock = NULL;
        }
        if (context) {
            JS_SetContextThread(context);
            JS_DestroyContextNoGC(context);
            context = NULL;
        }
        object = NULL;

        // Do not call parent->removeChild(). This is called either from
        // ~Worker, which calls it for us; or from parent->disposeChildren or
        // Worker::create, which require that it not be called.
        parent = NULL;
        disposeChildren();
    }

    static Worker *create(JSContext *parentcx, WorkerParent *parent,
                          JSString *scriptName, JSObject *obj);

    JSObject *asObject() { return object; }

    JSObject *getGlobal() { return JS_GetGlobalObject(context); }

    WorkerParent *getParent() { return parent; }

    virtual JSLock *getLock() { return lock; }

    virtual ThreadPool *getThreadPool() { return threadPool; }

    bool post(Event *event) {
        AutoLock hold(lock);
        if (terminated)
            return false;
        if (!current && events.empty() && !threadPool->getWorkerQueue()->post(this))
            return false;
        return events.push(event);
    }

    void setTerminateFlag() {
        AutoLock hold(lock);
        terminateFlag = true;
        if (current)
            JS_TriggerOperationCallback(context);
    }

    void processOneEvent();

    /* Trace method to be called from C++. */
    void trace(JSTracer *trc) {
        // Just mark the JSObject. If we haven't already been marked,
        // jsTraceWorker will be called, at which point we'll trace referents.
        JS_CALL_OBJECT_TRACER(trc, object, "queued Worker");
    }

    static bool getWorkerParentFromConstructor(JSContext *cx, JSObject *ctor, WorkerParent **p) {
        jsval v;
        if (!JS_GetReservedSlot(cx, ctor, 0, &v))
            return false;
        if (JSVAL_IS_VOID(v)) {
            // This means ctor is the root Worker constructor (created in
            // Worker::initWorkers as opposed to Worker::createContext, which sets up
            // Worker sandboxes) and nothing is initialized yet.
            if (!JS_GetReservedSlot(cx, ctor, 1, &v))
                return false;
            ThreadPool *threadPool = (ThreadPool *) JSVAL_TO_PRIVATE(v);
            if (!threadPool->start(cx))
                return false;
            WorkerParent *parent = threadPool->getMainQueue();
            if (!JS_SetReservedSlot(cx, ctor, 0, PRIVATE_TO_JSVAL(parent))) {
                threadPool->shutdown(cx);
                return false;
            }
            *p = parent;
            return true;
        }
        *p = (WorkerParent *) JSVAL_TO_PRIVATE(v);
        return true;
    }

    static JSBool jsConstruct(JSContext *cx, uintN argc, jsval *vp) {
        WorkerParent *parent;
        if (!getWorkerParentFromConstructor(cx, JSVAL_TO_OBJECT(JS_CALLEE(cx, vp)), &parent))
            return false;


        JSString *scriptName = JS_ValueToString(cx, argc ? JS_ARGV(cx, vp)[0] : JSVAL_VOID);
        if (!scriptName)
            return false;

        JSObject *obj = JS_NewObject(cx, &jsWorkerClass, NULL, NULL);
        if (!obj || !create(cx, parent, scriptName, obj))
            return false;
        JS_SET_RVAL(cx, vp, OBJECT_TO_JSVAL(obj));
        return true;
    }

    static JSFunctionSpec jsMethods[3];
    static JSFunctionSpec jsStaticMethod[2];

    static ThreadPool *initWorkers(JSContext *cx, WorkerHooks *hooks, JSObject *global,
                                   JSObject **objp) {
        // Create the ThreadPool object and its JSObject wrapper.
        ThreadPool *threadPool = ThreadPool::create(cx, hooks);
        if (!threadPool)
            return NULL;

        // Root the ThreadPool JSObject early.
        *objp = threadPool->asObject();

        // Create the Worker constructor.
        JSObject *proto = JS_InitClass(cx, global, NULL, &jsWorkerClass,
                                       jsConstruct, 1,
                                       NULL, jsMethods, NULL, NULL);
        if (!proto)
            return NULL;

        // Stash a pointer to the ThreadPool in constructor reserved slot 1.
        // It will be used later when lazily creating the MainQueue.
        JSObject *ctor = JS_GetConstructor(cx, proto);
        if (!JS_SetReservedSlot(cx, ctor, 1, PRIVATE_TO_JSVAL(threadPool)))
            return NULL;

        return threadPool;
    }
};

class InitEvent : public Event
{
  public:
    static InitEvent *create(JSContext *cx, Worker *worker, JSString *scriptName) {
        return createEvent<InitEvent>(cx, worker, worker, STRING_TO_JSVAL(scriptName));
    }

    Result process(JSContext *cx) {
        jsval s;
        if (!deserializeData(cx, &s))
            return fail;
        JS_ASSERT(JSVAL_IS_STRING(s));
        JSAutoByteString filename(cx, JSVAL_TO_STRING(s));
        if (!filename)
            return fail;

        JSScript *script = JS_CompileFile(cx, child->getGlobal(), filename.ptr());
        if (!script)
            return fail;

        AutoValueRooter rval(cx);
        JSBool ok = JS_ExecuteScript(cx, child->getGlobal(), script, Jsvalify(rval.addr()));
        JS_DestroyScript(cx, script);
        return Result(ok);
    }
};

class DownMessageEvent : public Event
{
  public:
    static DownMessageEvent *create(JSContext *cx, Worker *child, jsval data) {
        return createEvent<DownMessageEvent>(cx, child, child, data);
    }

    Result process(JSContext *cx) {
        return dispatch(cx, child->getGlobal(), "data", "onmessage", ok);
    }
};

class UpMessageEvent : public Event
{
  public:
    static UpMessageEvent *create(JSContext *cx, Worker *child, jsval data) {
        return createEvent<UpMessageEvent>(cx, child->getParent(), child, data);
    }

    Result process(JSContext *cx) {
        return dispatch(cx, child->asObject(), "data", "onmessage", ok);
    }
};

class ErrorEvent : public Event
{
  public:
    static ErrorEvent *create(JSContext *cx, Worker *child) {
        JSString *data = NULL;
        jsval exc;
        if (JS_GetPendingException(cx, &exc)) {
            AutoValueRooter tvr(cx, Valueify(exc));
            JS_ClearPendingException(cx);

            // Determine what error message to put in the error event.
            // If exc.message is a string, use that; otherwise use String(exc).
            // (This is a little different from what web workers do.)
            if (JSVAL_IS_OBJECT(exc)) {
                jsval msg;
                if (!JS_GetProperty(cx, JSVAL_TO_OBJECT(exc), "message", &msg))
                    JS_ClearPendingException(cx);
                else if (JSVAL_IS_STRING(msg))
                    data = JSVAL_TO_STRING(msg);
            }
            if (!data) {
                data = JS_ValueToString(cx, exc);
                if (!data)
                    return NULL;
            }
        }
        return createEvent<ErrorEvent>(cx, child->getParent(), child,
                                       data ? STRING_TO_JSVAL(data) : JSVAL_VOID);
    }

    Result process(JSContext *cx) {
        return dispatch(cx, child->asObject(), "message", "onerror", forwardToParent);
    }
};

} /* namespace workers */
} /* namespace js */

using namespace js::workers;

void
WorkerParent::disposeChildren()
{
    for (ChildSet::Enum e(children); !e.empty(); e.popFront()) {
        e.front()->dispose();
        e.removeFront();
    }
}

bool
MainQueue::shouldStop()
{
    // Note: This deliberately nests WorkerQueue::lock in MainQueue::lock.
    // Releasing MainQueue::lock would risk a race -- isIdle() could return
    // false, but the workers could become idle before we reacquire
    // MainQueue::lock and go to sleep, and we would wait on the condvar
    // forever.
    return closed || threadPool->getWorkerQueue()->isIdle();
}

void
MainQueue::trace(JSTracer *trc)
{
     JS_CALL_OBJECT_TRACER(trc, threadPool->asObject(), "MainQueue");
}

void
WorkerQueue::work() {
    AutoLock hold(lock);

    Worker *w;
    while (take(&w)) {  // can block outside the mutex
        JS_RELEASE_LOCK(lock);
        w->processOneEvent();     // enters request on w->context
        JS_ACQUIRE_LOCK(lock);
        drop(w);

        if (lockedIsIdle()) {
            JS_RELEASE_LOCK(lock);
            main->wake();
            JS_ACQUIRE_LOCK(lock);
        }
    }
}

const bool mswin =
#ifdef XP_WIN
    true
#else
    false
#endif
    ;

template <class Ch> bool
IsAbsolute(const Ch *filename)
{
    return filename[0] == '/' ||
           (mswin && (filename[0] == '\\' || (filename[0] != '\0' && filename[1] == ':')));
}

// Note: base is a filename, not a directory name.
static JSString *
ResolveRelativePath(JSContext *cx, const char *base, JSString *filename)
{
    size_t fileLen = JS_GetStringLength(filename);
    const jschar *fileChars = JS_GetStringCharsZ(cx, filename);
    if (!fileChars)
        return NULL;

    if (IsAbsolute(fileChars))
        return filename;

    // Strip off the filename part of base.
    size_t dirLen = -1;
    for (size_t i = 0; base[i]; i++) {
        if (base[i] == '/' || (mswin && base[i] == '\\'))
            dirLen = i;
    }

    // If base is relative and contains no directories, use filename unchanged.
    if (!IsAbsolute(base) && dirLen == (size_t) -1)
        return filename;

    // Otherwise return base[:dirLen + 1] + filename.
    js::Vector<jschar, 0, js::ContextAllocPolicy> result(cx);
    size_t nchars;
    if (!JS_DecodeBytes(cx, base, dirLen + 1, NULL, &nchars))
        return NULL;
    if (!result.reserve(dirLen + 1 + fileLen)) {
        JS_ReportOutOfMemory(cx);
        return NULL;
    }
    JS_ALWAYS_TRUE(result.resize(dirLen + 1));
    if (!JS_DecodeBytes(cx, base, dirLen + 1, result.begin(), &nchars))
        return NULL;
    JS_ALWAYS_TRUE(result.append(fileChars, fileLen));
    return JS_NewUCStringCopyN(cx, result.begin(), result.length());
}

Worker *
Worker::create(JSContext *parentcx, WorkerParent *parent, JSString *scriptName, JSObject *obj)
{
    Worker *w = new Worker();
    if (!w || !w->init(parentcx, parent, obj)) {
        delete w;
        return NULL;
    }

    JSStackFrame *frame = JS_GetScriptedCaller(parentcx, NULL);
    const char *base = JS_GetScriptFilename(parentcx, JS_GetFrameScript(parentcx, frame));
    JSString *scriptPath = ResolveRelativePath(parentcx, base, scriptName);
    if (!scriptPath)
        return NULL;

    // Post an InitEvent to run the initialization script.
    Event *event = InitEvent::create(parentcx, w, scriptPath);
    if (!event)
        return NULL;
    if (!w->events.push(event) || !w->threadPool->getWorkerQueue()->post(w)) {
        event->destroy(parentcx);
        JS_ReportOutOfMemory(parentcx);
        w->dispose();
        return NULL;
    }
    return w;
}

void
Worker::processOneEvent()
{
    Event *event;
    {
        AutoLock hold1(lock);
        if (lockedCheckTermination() || events.empty())
            return;

        event = current = events.pop();
    }

    JS_SetContextThread(context);
    JS_SetNativeStackQuota(context, gMaxStackSize);

    Event::Result result;
    {
        JSAutoRequest req(context);
        result = event->process(context);
    }

    // Note: we have to leave the above request before calling parent->post or
    // checkTermination, both of which acquire locks.
    if (result == Event::forwardToParent) {
        event->setChildAndRecipient(this, parent);
        if (parent->post(event)) {
            event = NULL;  // to prevent it from being deleted below
        } else {
            JS_ReportOutOfMemory(context);
            result = Event::fail;
        }
    }
    if (result == Event::fail && !checkTermination()) {
        JSAutoRequest req(context);
        Event *err = ErrorEvent::create(context, this);
        if (err && !parent->post(err)) {
            JS_ReportOutOfMemory(context);
            err->destroy(context);
            err = NULL;
        }
        if (!err) {
            // FIXME - out of memory, probably should panic
        }
    }

    if (event)
        event->destroy(context);
    JS_ClearContextThread(context);

    {
        AutoLock hold2(lock);
        current = NULL;
        if (!lockedCheckTermination() && !events.empty()) {
            // Re-enqueue this worker. OOM here effectively kills the worker.
            if (!threadPool->getWorkerQueue()->post(this))
                JS_ReportOutOfMemory(context);
        }
    }
}

JSBool
Worker::jsPostMessageToParent(JSContext *cx, uintN argc, jsval *vp)
{
    jsval workerval;
    if (!JS_GetReservedSlot(cx, JSVAL_TO_OBJECT(JS_CALLEE(cx, vp)), 0, &workerval))
        return false;
    Worker *w = (Worker *) JSVAL_TO_PRIVATE(workerval);

    {
        JSAutoSuspendRequest suspend(cx);  // avoid nesting w->lock in a request
        if (w->checkTermination())
            return false;
    }

    jsval data = argc > 0 ? JS_ARGV(cx, vp)[0] : JSVAL_VOID;
    Event *event = UpMessageEvent::create(cx, w, data);
    if (!event)
        return false;
    if (!w->parent->post(event)) {
        event->destroy(cx);
        JS_ReportOutOfMemory(cx);
        return false;
    }
    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return true;
}

JSBool
Worker::jsPostMessageToChild(JSContext *cx, uintN argc, jsval *vp)
{
    JSObject *workerobj = JS_THIS_OBJECT(cx, vp);
    if (!workerobj)
        return false;
    Worker *w = (Worker *) JS_GetInstancePrivate(cx, workerobj, &jsWorkerClass, JS_ARGV(cx, vp));
    if (!w) {
        if (!JS_IsExceptionPending(cx))
            JS_ReportError(cx, "Worker was shut down");
        return false;
    }
    
    jsval data = argc > 0 ? JS_ARGV(cx, vp)[0] : JSVAL_VOID;
    Event *event = DownMessageEvent::create(cx, w, data);
    if (!event)
        return false;
    if (!w->post(event)) {
        JS_ReportOutOfMemory(cx);
        return false;
    }
    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return true;
}

JSBool
Worker::jsTerminate(JSContext *cx, uintN argc, jsval *vp)
{
    JS_SET_RVAL(cx, vp, JSVAL_VOID);

    JSObject *workerobj = JS_THIS_OBJECT(cx, vp);
    if (!workerobj)
        return false;
    Worker *w = (Worker *) JS_GetInstancePrivate(cx, workerobj, &jsWorkerClass, JS_ARGV(cx, vp));
    if (!w)
        return !JS_IsExceptionPending(cx);  // ok to terminate twice

    JSAutoSuspendRequest suspend(cx);
    w->setTerminateFlag();
    return true;
}

void
Event::trace(JSTracer *trc)
{
    if (recipient)
        recipient->trace(trc);
    if (child)
        JS_CALL_OBJECT_TRACER(trc, child->asObject(), "worker");
}

JSClass ThreadPool::jsClass = {
    "ThreadPool", JSCLASS_HAS_PRIVATE | JSCLASS_MARK_IS_TRACE,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, jsFinalize,
    NULL, NULL, NULL, NULL,
    NULL, NULL, JS_CLASS_TRACE(jsTraceThreadPool), NULL
};

JSClass Worker::jsWorkerClass = {
    "Worker", JSCLASS_HAS_PRIVATE | JSCLASS_MARK_IS_TRACE,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, jsFinalize,
    NULL, NULL, NULL, NULL,
    NULL, NULL, JS_CLASS_TRACE(jsTraceWorker), NULL
};

JSFunctionSpec Worker::jsMethods[3] = {
    JS_FN("postMessage", Worker::jsPostMessageToChild, 1, 0),
    JS_FN("terminate", Worker::jsTerminate, 0, 0),
    JS_FS_END
};

ThreadPool *
js::workers::init(JSContext *cx, WorkerHooks *hooks, JSObject *global, JSObject **rootp)
{
    return Worker::initWorkers(cx, hooks, global, rootp);
}

void
js::workers::terminateAll(JSRuntime *rt, ThreadPool *tp)
{
    tp->terminateAll(rt);
}

void
js::workers::finish(JSContext *cx, ThreadPool *tp)
{
    if (MainQueue *mq = tp->getMainQueue()) {
        JS_ALWAYS_TRUE(mq->mainThreadWork(cx, true));
        tp->shutdown(cx);
    }
}

#endif /* JS_THREADSAFE */
