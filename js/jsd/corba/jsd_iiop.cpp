/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

/*
 * jsd_iiop.cpp   
 *
 * iiop interface to allow access to server backend from client frontend.
 *
 * John Bandhauer (jband@netscape.com)
 * 8/06/97
 *
 */
 
#ifndef SERVER_BUILD
    #error jsd_iiop.cpp is for LiveWire only  
#endif

#include "jsdebug.h"
#include "jsapi.h"
#include "jsstr.h"
#include "prhash.h"
#include "prmon.h"
#include "prlog.h"      /* for PR_ASSERT */
#include "prevent.h"
#include <base/pblock.h>
#include <base/session.h>
#include <frame/req.h>
#include <frame/log.h>
#include <frame/http.h>

#include "corba.h"
#include "NameLib.hpp"
#include "ifaces_s.hh"

// this shouldn't have to be here, but is not defined elsewhere
CosNaming::Name::~Name() {}

// #define JSD_IIOP_TRACE 1

#ifdef JSD_IIOP_TRACE
// quick hack...

static void _write_to_logfile(const char* str)
{
    static const char logfilename[] = "c:\\temp\\jsdiiop.log";
    static FILE* logfile = NULL;
    static PRMonitor* mon = NULL;

    if( NULL == mon && NULL == (mon = PR_NewMonitor()) )
        return;

    PR_EnterMonitor(mon);

    if( NULL == logfile && NULL == (logfile = fopen(logfilename, "a+")) )
    {
        PR_ExitMonitor(mon);
        return;
    }

#ifdef WIN32
    int threadid = (int) GetCurrentThreadId();
#else
    int threadid = (int) PR_GetCurrentThread();
#endif

    char* buf = PR_smprintf("thread: \"%0x\" msg: \"%s\"\n", threadid, str);
    fwrite(buf, sizeof(char), strlen(buf), logfile);        
    fflush(logfile);
    free(buf);
    PR_ExitMonitor(mon);
}    

// #define TRACE(msg) log_error(LOG_VERBOSE, "jsd_iiop", NULL, NULL, "TRACE: %s", msg)
#define TRACE(msg) _write_to_logfile(msg)
#else
#define TRACE(msg)
#endif

/***************************************************************************/
// deal with the fact that char* return values to CORBA implemented methods
// do not get freed!

static void _deferedFree(void* ptr)
{
// it is important that this number be high enough -- no recursive or 
// huge thread count usage of this function!
#define DEFERED_FREE_PTR_COUNT 50
    static void* cache[DEFERED_FREE_PTR_COUNT];
    static current = 0;
    static PRMonitor* mon = NULL;

    if( NULL == mon )
    {
        // init the monitor
        mon = PR_NewMonitor();
        if( NULL == mon )
            return;
        PR_EnterMonitor(mon);
        // belt and suspenders :)
        for( int i = 0; i < DEFERED_FREE_PTR_COUNT; i++ )
            cache[i] = NULL;
    }
    else
        PR_EnterMonitor(mon);

    if( cache[current] )
        free( cache[current] );
    cache[current] = ptr;

    current++ ;
    if( current == DEFERED_FREE_PTR_COUNT )
        current = 0;

    PR_ExitMonitor(mon);
}    



/***************************************************************************/
//
// local declarations
//
// prototype these for proper extern 'C'
PR_BEGIN_EXTERN_C

PR_EXTERN(int)
ssjsdebugInit(pblock *pb, Session *sn, Request *rq);

PR_EXTERN(PRUintn)
ssjsdErrorReporter( JSDContext*     jsdc, 
                    JSContext*      cx,
                    const char*     message, 
                    JSErrorReport*  report,
                    void*           callerdata );

PR_EXTERN(void)
ssjsdScriptHookProc( JSDContext* jsdc, 
                     JSDScript*  jsdscript,
                     JSBool      creating,
                     void*       callerdata );

PR_EXTERN(PRUintn)
ssjsdExecutionHookProc( JSDContext*     jsdc, 
                        JSDThreadState* jsdthreadstate,
                        PRUintn         type,
                        void*           callerdata );

PR_END_EXTERN_C

/**************************/

static PRMonitor* _jsd_iiop_monitor = NULL;
#define LOCK()                                                              \
    PR_BEGIN_MACRO                                                          \
        if( ! _jsd_iiop_monitor )                                           \
            _jsd_iiop_monitor = PR_NewMonitor();                            \
        PR_ASSERT(_jsd_iiop_monitor);                                       \
        PR_EnterMonitor(_jsd_iiop_monitor);                                 \
    PR_END_MACRO

#define UNLOCK()                                                            \
    PR_BEGIN_MACRO                                                          \
        PR_ASSERT(_jsd_iiop_monitor);                                       \
        PR_ExitMonitor(_jsd_iiop_monitor);                                  \
    PR_END_MACRO

/**************************/

class TestInterface_impl : public _sk_TestInterface
{
public:
    TestInterface_impl() : _sk_TestInterface("bogus") {}
    virtual char*   getFirstAppInList();
    virtual void    getAppNames(StringReciever_ptr sr);
	virtual TestInterface::sequence_of_Thing * getThings();
	virtual void callBounce(StringReciever_ptr arg0, CORBA::Long arg1);
};

/**************************/

class SourceTextProvider : public _sk_ISourceTextProvider
{
public:
    SourceTextProvider(const char* name);
    SourceTextProvider(const char* name, JSDContext* jsdc);
    virtual ~SourceTextProvider();

    void init(JSDContext* jsdc);

    // implement ISourceTextProvider
	virtual ISourceTextProvider::sequence_of_string * getAllPages();
	virtual void refreshAllPages();
	virtual CORBA::Boolean hasPage(const char * arg0);
	virtual CORBA::Boolean loadPage(const char * arg0);
	virtual void refreshPage(const char * arg0);
	virtual char * getPageText(const char * arg0);
	virtual CORBA::Long getPageStatus(const char * arg0);
	virtual CORBA::Long getPageAlterCount(const char * arg0);

protected:
    void    _forcePreLoad();
    void    _lock()   {JSD_LockSourceTextSubsystem(_jsdc);}
    void    _unlock() {JSD_UnlockSourceTextSubsystem(_jsdc);}

private:
    JSDContext*             _jsdc;
};

/**************************/

class HookKey
{
public:
    HookKey(const IJSPC& pc);
    IJSPC        pc;
    PRHashNumber hash_num;
};

/**************************/

// forward declaration...
class StepHandler;

/**************************/

class DebugController : public _sk_IDebugController
{
public:
    DebugController(const char* name);
    DebugController(const char* name, JSDContext* jsdc);
    virtual ~DebugController();

    void init(JSDContext* jsdc);

	// implement IDebugController
	virtual CORBA::Long getMajorVersion() {return JSD_GetMajorVersion();}
	virtual CORBA::Long getMinorVersion() {return JSD_GetMinorVersion();}

	virtual IJSErrorReporter_ptr setErrorReporter(IJSErrorReporter_ptr arg0);
	virtual IJSErrorReporter_ptr getErrorReporter();

	virtual IScriptHook_ptr setScriptHook(IScriptHook_ptr arg0);
	virtual IScriptHook_ptr getScriptHook();

	virtual IJSPC * getClosestPC(const IScript& arg0, CORBA::Long arg1);

	virtual IJSSourceLocation * getSourceLocation(const IJSPC& arg0);

	virtual IJSExecutionHook_ptr setInterruptHook(IJSExecutionHook_ptr arg0);
	virtual IJSExecutionHook_ptr getInterruptHook();

	virtual IJSExecutionHook_ptr setDebugBreakHook(IJSExecutionHook_ptr arg0);
	virtual IJSExecutionHook_ptr getDebugBreakHook();

	virtual IJSExecutionHook_ptr setInstructionHook(IJSExecutionHook_ptr arg0,
                                                    const IJSPC& arg1);
	virtual IJSExecutionHook_ptr getInstructionHook(const IJSPC& arg0);

	virtual CORBA::Boolean isRunningHook(CORBA::Long arg0);
	virtual CORBA::Boolean isWaitingForResume(CORBA::Long arg0);
	virtual void setThreadContinueState(CORBA::Long arg0, CORBA::Long arg1);
	virtual void setThreadReturnValue(CORBA::Long arg0, const char * arg1);

	virtual void sendInterrupt();

	virtual void sendInterruptStepInto(CORBA::Long arg0);
	virtual void sendInterruptStepOver(CORBA::Long arg0);
	virtual void sendInterruptStepOut(CORBA::Long arg0);
	virtual void reinstateStepper(CORBA::Long arg0);

	virtual IExecResult * executeScriptInStackFrame(CORBA::Long arg0,
			const IJSStackFrameInfo& arg1,
			const char * arg2,
			const char * arg3,
			CORBA::Long arg4);
	virtual void leaveThreadSuspended(CORBA::Long arg0);
	virtual void resumeThread(CORBA::Long arg0);


	virtual void iterateScripts(IScriptHook_ptr arg0);

public:
    // these are called by extern 'C' callbacks (otherwise would be private)
    int     _callErrorReporter(const char* msg, JSErrorReport* report);
    void    _callScriptHook(JSDScript* jsdscript, JSBool creating);
    int     _callExecutionHook(JSDThreadState* jsdthreadstate, PRUintn type);

private:
    void _initScript(IScript& s, CORBA::Long jsdscript, PRBool alive);

    inline void _initPC(IJSPC& pc, const IScript& script, CORBA::Long offset)
    {
        pc.script = script; 
        pc.offset = offset;
    }
    inline void _initSourceLocation(IJSSourceLocation& sl, 
                                    const IJSPC& pc,
                                    CORBA::Long line)
    {
        sl.pc = pc;
        sl.line = line; 
    }
    inline void _initStackFrameInfo(IJSStackFrameInfo& sf, const IJSPC& pc,
                                    CORBA::Long jsdframe)
    {
        sf.pc = pc; 
        sf.jsdframe = jsdframe;
    }

    IJSExecutionHook_ptr _findInstructionHook(const IJSPC& pc);
    PRBool               _addInstructionHook(const IJSPC& pc, 
                                             IJSExecutionHook_ptr hook);
    IJSExecutionHook_ptr _removeInstructionHook(const IJSPC& pc);
    PRBool               _initInstructionHookTable();    
    void                 _freeInstructionHookTable();    

    inline void    _lock()   {PR_EnterMonitor(_monitor);}
    inline void    _unlock() {PR_ExitMonitor(_monitor);}

    JSDScript*     _findScriptAt(const char* filename, int lineno);
    void           _clearStepHandler(PRBool canReinstate);
    void           _reinstateStepHandler();

    // data...

private:
    JSDContext*             _jsdc;
    IJSErrorReporter_ptr    _errorReporter;
    IScriptHook_ptr         _scriptHook;
    IJSExecutionHook_ptr    _interruptHook;
    IJSExecutionHook_ptr    _debugBreakHook;
    PRHashTable *           _instructionHookTable;
    PRMonitor*              _monitor;
    StepHandler*            _stepHandler;
    StepHandler*            _oldStepHandler;

};

/**************************/

class ThreadState
{
private:
    // only our static can construct one of us
    ThreadState();  // not implemented
    ThreadState(PRThread* currentThread);      
    ~ThreadState();
public:
    static ThreadState* getThreadStateForCurrentThread(void);
    static ThreadState* findThreadStateForCurrentThread(void);
        
    void    callHook(DebugController*               controller,
                     IJSExecutionHook*              hook,
                     sequence_of_IJSStackFrameInfo& stack,
                     IJSPC*                         pcTop,
                     JSDThreadState*                jsdthreadstate);

    PRBool  isRunningHook(void);
    PRBool  isWaitingForResume(void);
    void    leaveThreadSuspended(void);
    void    resumeThread();

    inline PREventQueue* getEventQueue(void) {return  _mainEventQueue;}
    inline IJSThreadState& getJSThreadState(void) {return _jsts;}

    // these (especially set) should be called on this thread's ThreadState
    PRBool  getRunningEval(void) {return _isRunningEval;}
    void    setRunningEval(PRBool b) {_isRunningEval = b;}
    IExecResult& getExecResult(void) {return _execResult;}


public:
    // these should be called by the appropriate Events only...
    void eventSaysSuicide(void);
    void eventSaysResume(void);
    void eventSaysCallHook(void);

private:
    // private statics...
    inline static void _lock()
    {
        if(! _monitor)
            _monitor = PR_NewMonitor();
        PR_ASSERT(_monitor);
        PR_EnterMonitor(_monitor);
    }
    inline static void _unlock()
    {
        PR_ASSERT(_monitor);
        PR_ExitMonitor(_monitor);
    }
    inline static void _wait()
    {
        PR_ASSERT(_monitor);
        PR_Wait(_monitor, PR_INTERVAL_NO_TIMEOUT);
    }
    inline static void _notify()
    {
        PR_ASSERT(_monitor);
        PR_Notify(_monitor);
    }

    static void _otherThreadProc(void* arg);
    static void _callbackForPRThreadDeath(void *arg);

    static PRHashNumber _hash_root(const void *key);
    static PRBool _prepThread2IndexTable();
    static PRBool _findIndexForThread(PRThread* thread, PRUintn* index);
    static PRBool _setIndexForThread(PRThread* thread, PRUintn index);
    static PRBool _removeIndexForThread(PRThread* thread);

    static ThreadState* _findThreadState(PRThread* currentThread);
    static ThreadState* _createThreadState(PRThread* currentThread);
    
private:
    // private instance methods

    PRBool _initEventQueues(void);

private:
    static PRMonitor*   _monitor;
    static PRHashTable* _thread2IndexTable;

    PRThread*           _currentThread;
    DebugController*    _controller;
    IJSExecutionHook*   _hook;
    IJSThreadState      _jsts;
    IJSPC*              _pcTop;

    PRBool              _isRunningEval;
    IExecResult         _execResult;

    PRBool              _isRunningHook;
    PRBool              _actualCallToHookIsHappening;
    PRBool              _leaveSuspended;

    PREventQueue*       _mainEventQueue;
    PREventQueue*       _hookCallerEventQueue;

    PRBool              _hookCallerEventQueueCreateFinished;
    PRBool              _suicideMessageReceived;
};

/***************************************************************************/
// These Event classes all MUST cast to PREvent. So....
//   1) no virtual functions
//   2) PREvent must be first in struct OR class must inherit from PREvent

class JSDResumeEvent : public PREvent
{
public:
    JSDResumeEvent(ThreadState* ts);
    void post(PREventQueue* queue);

    static void* static_handle(PREvent* e);
    static void  static_destroy(PREvent* e);
private:
    ThreadState* _ts;
};

/**************************/

class JSDSuicideEvent : public PREvent
{
public:
    JSDSuicideEvent(ThreadState* ts);
    void post(PREventQueue* queue);

    static void* static_handle(PREvent* e);
    static void  static_destroy(PREvent* e);
private:
    ThreadState* _ts;
}; 

/**************************/

class JSDCallHookEvent : public PREvent
{
public:
    JSDCallHookEvent(ThreadState* ts);
    void post(PREventQueue* queue);

    static void* static_handle(PREvent* e);
    static void  static_destroy(PREvent* e);
private:
    ThreadState*         _ts;
};

/**************************/

class JSDExecuteEvent : public PREvent
{
public:
    JSDExecuteEvent(JSDContext*              jsdc,
                    ThreadState*             ts,
                    const IJSStackFrameInfo& frame,
                    const char*              text,
                    const char*              filename,
                    int                      lineno);
    ~JSDExecuteEvent();

    IExecResult* postSynchronous(PREventQueue* queue);

    static void* static_handle(PREvent* e);
    static void  static_destroy(PREvent* e);

private:
    void* _handle();

private:
    JSDContext*         _jsdc;
    ThreadState*        _ts;
    IJSStackFrameInfo   _frame;
    char*               _text;
    char*               _filename;
    int                 _lineno;
};

/**************************/
// support for stepping...

class CallChain
{
public:
    enum CompareResult
    {
        EQUAL,
        CALLER,
        CALLEE,
        DISJOINT
    };

    CallChain();    // not implemented
    CallChain(JSDContext* jsdc, JSDThreadState* jsdthreadstate);
    ~CallChain();

    CallChain::CompareResult compare(const CallChain& other);

    // data...
private:
    JSDContext* _jsdc;
    JSDScript**  _chain;
    int         _count;
};

/************************/

class StepHandler
{
public:

    enum StepResult
    {
        STOP,
        CONTINUE_SEND_INTERRUPT,
        CONTINUE_DONE          
    };

    StepHandler();  // not implemented
    StepHandler(JSDContext* jsdc, ThreadState* state, PRBool buildCallChain);
    ~StepHandler();

    virtual StepHandler::StepResult step(JSDThreadState* jsdthreadstate) = 0;

protected:
    JSDStackFrameInfo* _topFrame(JSDThreadState* jsdthreadstate)
    {
        return JSD_GetStackFrame(_jsdc, jsdthreadstate);
    }

    JSDScript* _topScript(JSDThreadState* jsdthreadstate, JSDStackFrameInfo* jsdframe)
    {
        return JSD_GetScriptForStackFrame(_jsdc, jsdthreadstate, jsdframe);
    }

    prword_t _topPC(JSDThreadState* jsdthreadstate, JSDStackFrameInfo* jsdframe)
    {
        return JSD_GetPCForStackFrame(_jsdc, jsdthreadstate, jsdframe);
    }

    int _topLine(JSDScript* jsdscript, prword_t pc)
    {
        return JSD_GetClosestLine(_jsdc, jsdscript, pc);
    }


    // data...
protected:
    JSDContext*     _jsdc;
    JSDScript*      topScriptInitial;
    prword_t        topPCInitial;
    int             topLineInitial;
    CallChain*      _callChain;
};

/************************/

class StepInto : public StepHandler
{
public:
    StepInto(JSDContext* jsdc, ThreadState* state);

    StepHandler::StepResult step(JSDThreadState* jsdthreadstate);

private:
};

/************************/

class StepOver : public StepHandler
{
public:
    StepOver(JSDContext* jsdc, ThreadState* state);

    StepHandler::StepResult step(JSDThreadState* jsdthreadstate);

private:
};

/************************/

class StepOut : public StepHandler
{
public:
    StepOut(JSDContext* jsdc, ThreadState* state);

    StepHandler::StepResult step(JSDThreadState* jsdthreadstate);

private:
};

/***************************************************************************/
/***************************************************************************/

PR_IMPLEMENT(PRUintn)
ssjsdErrorReporter( JSDContext*     jsdc, 
                    JSContext*      cx,
                    const char*     message, 
                    JSErrorReport*  report,
                    void*           callerdata )
{
    DebugController* controller = (DebugController*) callerdata;
    return (PRUintn) controller->_callErrorReporter(message, report);
}

PR_IMPLEMENT(void)
ssjsdScriptHookProc( JSDContext* jsdc, 
                     JSDScript*  jsdscript,
                     JSBool      creating,
                     void*       callerdata )
{
    DebugController* controller = (DebugController*) callerdata;
    controller->_callScriptHook(jsdscript, creating);
}

PR_IMPLEMENT(PRUintn)
ssjsdExecutionHookProc( JSDContext*     jsdc, 
                        JSDThreadState* jsdthreadstate,
                        PRUintn         type,
                        void*           callerdata )
{
    DebugController* controller = (DebugController*) callerdata;
    return (PRUintn) controller->_callExecutionHook(jsdthreadstate, type);
}


DebugController::DebugController(const char* name, JSDContext* jsdc)
    :   _sk_IDebugController(name),
        _jsdc(NULL),
        _errorReporter(NULL),
        _scriptHook(NULL),
        _interruptHook(NULL),
        _debugBreakHook(NULL),
        _instructionHookTable(NULL),
        _stepHandler(NULL),
        _oldStepHandler(NULL)
{
    init(jsdc);
}

DebugController::DebugController(const char* name)
    :   _sk_IDebugController(name),
        _jsdc(NULL),
        _errorReporter(NULL),
        _scriptHook(NULL),
        _interruptHook(NULL),
        _debugBreakHook(NULL),
        _instructionHookTable(NULL),
        _stepHandler(NULL),
        _oldStepHandler(NULL)
{
    // do nothing    
}    

void
DebugController::init(JSDContext* jsdc)
{
    _jsdc = jsdc;
    _monitor = PR_NewMonitor();
    JSD_SetErrorReporter(_jsdc, ssjsdErrorReporter, this);
    JSD_SetScriptHook(_jsdc, ssjsdScriptHookProc, this);
    JSD_SetDebugBreakHook(_jsdc, ssjsdExecutionHookProc, this );
    _initInstructionHookTable();
}    

DebugController::~DebugController()
{
    if( _jsdc )
    {
        JSD_SetErrorReporter(_jsdc, NULL, NULL);
        JSD_SetScriptHook(_jsdc, NULL, NULL);
        JSD_SetDebugBreakHook(_jsdc, NULL, NULL );
        _freeInstructionHookTable();
    }
    if(_monitor)
        PR_DestroyMonitor(_monitor);
}    

IJSErrorReporter_ptr 
DebugController::setErrorReporter(IJSErrorReporter_ptr arg0)
{
    TRACE("DebugController::setErrorReporter called");
    IJSErrorReporter_ptr old = _errorReporter;
    _errorReporter = arg0;

    if( _errorReporter )
        _errorReporter->_duplicate(_errorReporter);

    // no need to release the old hook, because returning it here
    // will cause a release call.
    return old;
}    

IJSErrorReporter_ptr 
DebugController::getErrorReporter()
{
    TRACE("DebugController::getErrorReporter called");
    if( _errorReporter )
        return _errorReporter->_duplicate(_errorReporter);
    return NULL;    
}    

IScriptHook_ptr 
DebugController::setScriptHook(IScriptHook_ptr arg0)
{
    TRACE("DebugController::setScriptHook called");
    IScriptHook_ptr old = _scriptHook;
    _scriptHook = arg0;

    if( _scriptHook )
        _scriptHook->_duplicate(_scriptHook);

    // no need to release the old hook, because returning it here
    // will cause a release call.
    return old;
}    

IScriptHook_ptr 
DebugController::getScriptHook()
{
    TRACE("DebugController::getScriptHook called");
    if( _scriptHook )
        return _scriptHook->_duplicate(_scriptHook);
    return NULL;
}    

IJSExecutionHook_ptr 
DebugController::setInterruptHook(IJSExecutionHook_ptr arg0)
{
    TRACE("DebugController::setInterruptHook called");
    IJSExecutionHook_ptr old = _interruptHook;
    _interruptHook = arg0;

    if( _interruptHook )
        _interruptHook->_duplicate(_interruptHook);

    // no need to release the old hook, because returning it here
    // will cause a release call.
    return old;
}

IJSExecutionHook_ptr 
DebugController::getInterruptHook()
{
    TRACE("DebugController::getInterruptHook called");
    if( _interruptHook )
        return _interruptHook->_duplicate(_interruptHook);
    return NULL;
}

IJSExecutionHook_ptr 
DebugController::setDebugBreakHook(IJSExecutionHook_ptr arg0)
{
    TRACE("DebugController::setDebugBreakHook called");
    IJSExecutionHook_ptr old = _debugBreakHook;
    _debugBreakHook = arg0;

    if( _debugBreakHook )
        _debugBreakHook->_duplicate(_debugBreakHook);

    // no need to release the old hook, because returning it here
    // will cause a release call.
    return old;
}

IJSExecutionHook_ptr 
DebugController::getDebugBreakHook()
{
    TRACE("DebugController::getDebugBreakHook called");
    if( _debugBreakHook )
        return _debugBreakHook->_duplicate(_debugBreakHook);
    return NULL;
}


IJSExecutionHook_ptr 
DebugController::setInstructionHook(IJSExecutionHook_ptr arg0,
                                    const IJSPC& arg1)
{
    TRACE("DebugController::setInstructionHook called");
    IJSExecutionHook_ptr oldHook = _findInstructionHook(arg1);
    if( oldHook )
        _removeInstructionHook(arg1);

    if(arg0)
    {
        arg0->_duplicate(arg0);
        _addInstructionHook(arg1, arg0);
    }

    // no need to release the old hook, because returning it here
    // will cause a release call.
    return oldHook;
}

IJSExecutionHook_ptr 
DebugController::getInstructionHook(const IJSPC& arg0)
{
    TRACE("DebugController::getInstructionHook() called");
    IJSExecutionHook_ptr hook = _findInstructionHook(arg0);
    if( hook )
        return hook->_duplicate(hook);
    return NULL;
}

IJSPC*
DebugController::getClosestPC(const IScript& arg0, CORBA::Long arg1)
{
    TRACE("DebugController::getClosestPC() called");
    PRUintn offset = JSD_GetClosestPC(_jsdc, (JSDScript*)arg0.jsdscript, arg1);
    IJSPC* pc = new IJSPC();
    _initPC(*pc, arg0, (CORBA::Long) offset);
    return pc;
}

IJSSourceLocation * 
DebugController::getSourceLocation(const IJSPC& arg0)
{
    TRACE("DebugController::getSourceLocation() called");
    JSDScript* jsdscript = (JSDScript*)arg0.script.jsdscript;
    int line   = JSD_GetClosestLine(_jsdc, jsdscript, (PRUintn) arg0.offset);
    int offset = JSD_GetClosestPC(_jsdc, jsdscript, line);

    IJSPC pc;
    _initPC(pc, arg0.script, (CORBA::Long) offset);
    
    IJSSourceLocation* loc = new IJSSourceLocation();
    _initSourceLocation(*loc, pc, (CORBA::Long)line);
    return loc;
} 

void 
DebugController::setThreadContinueState(CORBA::Long arg0, CORBA::Long arg1)
{
    TRACE("DebugController::setThreadContinueState() called");
    ThreadState* state = (ThreadState*)arg0;
    state->getJSThreadState().continueState = arg1;
}

void 
DebugController::setThreadReturnValue(CORBA::Long arg0, const char * arg1)
{
    TRACE("DebugController::setThreadReturnValue() called");
    ThreadState* state = (ThreadState*)arg0;
    state->getJSThreadState().returnValue = arg1;
}
 
void 
DebugController::sendInterrupt()
{
    TRACE("DebugController::sendInterrupt() called");
    _clearStepHandler(PR_FALSE);
    JSD_SetInterruptHook(_jsdc, ssjsdExecutionHookProc, this );
}

void DebugController::sendInterruptStepInto(CORBA::Long arg0)
{
    TRACE("DebugController::sendInterruptStepInto() called");
    _clearStepHandler(PR_FALSE);
    _stepHandler = new StepInto(_jsdc, (ThreadState*) arg0);
    JSD_SetInterruptHook(_jsdc, ssjsdExecutionHookProc, this );
}    

void DebugController::sendInterruptStepOver(CORBA::Long arg0)
{
    TRACE("DebugController::sendInterruptStepOver() called");
    _clearStepHandler(PR_FALSE);
    _stepHandler = new StepOver(_jsdc, (ThreadState*) arg0);
    JSD_SetInterruptHook(_jsdc, ssjsdExecutionHookProc, this );
}    

void DebugController::sendInterruptStepOut(CORBA::Long arg0)
{
    TRACE("DebugController::sendInterruptStepOut() called");
    _clearStepHandler(PR_FALSE);
    _stepHandler = new StepOut(_jsdc, (ThreadState*) arg0);
    JSD_SetInterruptHook(_jsdc, ssjsdExecutionHookProc, this );
}

void DebugController::reinstateStepper(CORBA::Long arg0)
{
    TRACE("DebugController::reinstateStepper() called");
    _reinstateStepHandler();
}    

IExecResult *
DebugController::executeScriptInStackFrame(CORBA::Long arg0,
                                           const IJSStackFrameInfo& arg1,
                                           const char * arg2,
                                           const char * arg3,
                                           CORBA::Long arg4)
{
    TRACE("DebugController::executeScriptInStackFrame() called");
    ThreadState* state = (ThreadState*) arg0;

    if( ! state->isRunningHook() )
    {
        PR_ASSERT(0);
        return 0;
    }

    JSDExecuteEvent* e = new JSDExecuteEvent(_jsdc,state,arg1,arg2,arg3,arg4);
    return e->postSynchronous(state->getEventQueue());
}


CORBA::Boolean 
DebugController::isRunningHook(CORBA::Long arg0)
{
    TRACE("DebugController::isRunningHook() called");
    ThreadState* state = (ThreadState*) arg0;
    return state->isRunningHook();
}

CORBA::Boolean 
DebugController::isWaitingForResume(CORBA::Long arg0)
{
    TRACE("DebugController::isWaitingForResume() called");
    ThreadState* state = (ThreadState*) arg0;
    return state->isWaitingForResume();
}

void
DebugController::leaveThreadSuspended(CORBA::Long arg0)
{
    TRACE("DebugController::leaveThreadSuspended() called");
    ThreadState* state = (ThreadState*) arg0;
    state->leaveThreadSuspended();
}    

void
DebugController::resumeThread(CORBA::Long arg0)
{
    TRACE("DebugController::resumeThread() called");
    ThreadState* state = (ThreadState*) arg0;
    state->resumeThread();
}    

void 
DebugController::iterateScripts(IScriptHook_ptr arg0)
{
    IScript script;
    JSDScript* jsdscript;
    JSDScript* iter = NULL;

    JSD_LockScriptSubsystem(_jsdc);
    while( NULL != (jsdscript = JSD_IterateScripts(_jsdc, &iter)) )
    {
        _initScript(script, (long) jsdscript, PR_TRUE);
        try
        {
            TRACE("+++HOOK arg0->justLoadedScript() call hook");
            arg0->justLoadedScript(script);
            TRACE("---HOOK arg0->justLoadedScript() call hook");
        }
        catch(...)
        {
            // eat it...        
        }
    }
    JSD_UnlockScriptSubsystem(_jsdc);
}    


/**************************************/

void 
DebugController::_initScript(IScript& s, CORBA::Long jsdscript, PRBool alive)
{
    s.url     = JSD_GetScriptFilename(_jsdc, (JSDScript*)jsdscript);
    s.funname = JSD_GetScriptFunctionName(_jsdc, (JSDScript*)jsdscript);
    s.base    = JSD_GetScriptBaseLineNumber(_jsdc, (JSDScript*)jsdscript);
    s.extent  = JSD_GetScriptLineExtent(_jsdc, (JSDScript*)jsdscript);
    s.jsdscript = jsdscript;

    // init the sections...

    LWDBGScript* lwscript = JSDLW_GetLWScript(_jsdc, (JSDScript*)jsdscript);
    PRUintn count = 0;

    // iterate once to get the count...
    // BUT: don't iterate if the script is being destroyed...
    if( alive )
    {
        PRUintn iter = 0;
        while( LWDBG_ScriptSectionIterator(lwscript,NULL,NULL,NULL,&iter) )
            count++;
    }

    // if sections exist, then grab those with 'user' code

    PRBool addedAtLeastOneSection = PR_FALSE;
    if( count )
        {
        IScriptSection* sections = new IScriptSection[count];
        int index = 0;
        PRUintn base = s.base;
        PRUintn extent;
        PRUintn type;
        PRUintn iter = 0;
        while( LWDBG_ScriptSectionIterator(lwscript,&type,&extent,NULL,&iter) )
        {
            PRBool hasCode;
            switch( type )
            {
                case LWDBG_SECTIONTYPE_OTHER:
                    hasCode = PR_FALSE;
                    break;
                case LWDBG_SECTIONTYPE_SERVER:
                case LWDBG_SECTIONTYPE_BACKQUOTE:
                case LWDBG_SECTIONTYPE_EQBACKQUOTE:
                    hasCode = PR_TRUE;
                    break;
                case LWDBG_SECTIONTYPE_URL:
                default:
                    hasCode = PR_FALSE;
                    break;
            }

            if( hasCode )
            {
                sections[index].base   = base;
                sections[index].extent = extent+1;  // XXX hackage...
                index++;
            }
            base += extent;
        }
        if( index )
        {
            s.sections = sequence_of_IScriptSection(count,index,sections,1);
            addedAtLeastOneSection = PR_TRUE;
        }
        else
        {
            delete [] sections;
        }
    }
    if( ! addedAtLeastOneSection )
    {
        PRUintn baseBefore = s.base;
        if(alive)
            JSDLW_ProcessedToRawLineNumber(_jsdc, (JSDScript*)jsdscript, 
                                           s.base, &baseBefore );
        // no sections added, add the whole thing as a single section
        IScriptSection* sections = new IScriptSection[1];
        sections[0].base   = baseBefore;
        sections[0].extent = s.extent;
        s.sections = sequence_of_IScriptSection(1,1,sections,1);
    }
}

int 
DebugController::_callErrorReporter(const char* msg, JSErrorReport* report)
{
    if( ! _errorReporter )
        return JSD_ERROR_REPORTER_PASS_ALONG;

    const char*  filename    = NULL;
    unsigned int lineno      = 0;
    const char*  linebuf     = NULL;
    int          tokenOffset = 0;
    int          retval      = JSD_ERROR_REPORTER_PASS_ALONG;

    if( report )
    {
        filename = report->filename;
        linebuf  = report->linebuf;
        lineno   = report->lineno;
        if( report->linebuf && report->tokenptr )
            tokenOffset = report->tokenptr - report->linebuf;

        // convert linenumber to Raw
        JSDScript* jsdscript = _findScriptAt(filename, lineno);
        if( jsdscript )
            JSDLW_ProcessedToRawLineNumber(_jsdc, jsdscript, lineno, &lineno);
    }


    // if we are doing an eval on this thread, then gather the info from the
    // report and then tell JSD to ignore it.

    ThreadState* ts = ThreadState::findThreadStateForCurrentThread();
    if( ts && ts->getRunningEval() )
    {
        IExecResult& result = ts->getExecResult();

        result.errorOccured     = 1;
        result.errorMessage     = (const char*) msg;
        result.errorFilename    = (const char*) filename;
        result.errorLineNumber  = lineno;
        result.errorLineBuffer  = (const char*) linebuf;
        result.errorTokenOffset = tokenOffset;

        return JSD_ERROR_REPORTER_RETURN;
    }
    // else....

    try
    {
        TRACE("+++HOOK _errorReporter->reportError() call hook");
        retval = _errorReporter->reportError(msg,filename,lineno,linebuf,tokenOffset);
        TRACE("---HOOK _errorReporter->reportError() call hook");
    }
    catch(...)
    {
        // eat it...        
    }
    return retval;
}

void    
DebugController::_callScriptHook(JSDScript* jsdscript, JSBool creating)
{
    if( ! _scriptHook )
        return;

    // The debugger does not need to know about scripts created and destroyed
    // while doing eval on that thread (which is what must be happening if
    // we have the thread stopped, yet have reaced this hook)
    //
    // This seemed to be causing deadlocks in calling back to the debugger 
    // under some circumstances

    ThreadState* ts = ThreadState::findThreadStateForCurrentThread();
    if( ts && ts->isRunningHook() )
        return;

    IScript script;
    _initScript(script, (long) jsdscript, creating);
    
    try
    {
        if( creating )
        {
            TRACE("+++HOOK _scriptHook->justLoadedScript() call hook");
            _scriptHook->justLoadedScript(script);
            TRACE("---HOOK _scriptHook->justLoadedScript() call hook");
        }
        else
        {
            TRACE("+++HOOK _scriptHook->aboutToUnloadScript() call hook");
            _scriptHook->aboutToUnloadScript(script);
            TRACE("---HOOK _scriptHook->aboutToUnloadScript() call hook");
        }
    }
    catch(...)
    {
        // eat it...        
    }
}    

int     
DebugController::_callExecutionHook(JSDThreadState* jsdthreadstate, PRUintn type)
{
    IJSExecutionHook_ptr hook = NULL;

    JSDStackFrameInfo* jsdframeTop = JSD_GetStackFrame(_jsdc, jsdthreadstate);
    IScript scriptTop;
    _initScript(scriptTop, (CORBA::Long)
                JSD_GetScriptForStackFrame(_jsdc, jsdthreadstate,jsdframeTop),
                PR_TRUE );
    IJSPC pcTop;
    _initPC(pcTop, scriptTop, (CORBA::Long) 
            JSD_GetPCForStackFrame(_jsdc, jsdthreadstate, jsdframeTop));

    switch(type)
    {
    case JSD_HOOK_INTERRUPTED:
        hook = _interruptHook;
        if( _stepHandler )
        {
            switch( _stepHandler->step(jsdthreadstate) )
            {
                case StepHandler::STOP:
                    _clearStepHandler(PR_FALSE);
                    break;
                case StepHandler::CONTINUE_SEND_INTERRUPT:
                    hook = NULL;
                    break;
                case StepHandler::CONTINUE_DONE:
                    _clearStepHandler(PR_FALSE);
                    hook = NULL;
                    break;
                default:
                    PR_ASSERT(0);
            }
        }
        else
        {
            JSD_ClearInterruptHook(_jsdc); 
        }
        break;
    case JSD_HOOK_BREAKPOINT:        
        _clearStepHandler(PR_TRUE);
        JSD_ClearInterruptHook(_jsdc); 
        hook = _findInstructionHook(pcTop);
        break;
    case JSD_HOOK_DEBUG_REQUESTED:
        _clearStepHandler(PR_FALSE);
        JSD_ClearInterruptHook(_jsdc); 
        hook = _debugBreakHook;
        break;
    default:
        // invalid type!
        PR_ASSERT(0);
        break;
    }

    if( ! hook )
        return JSD_HOOK_RETURN_CONTINUE;

    // generate the stack (reverse the order)
    int count = JSD_GetCountOfStackFrames(_jsdc, jsdthreadstate);
    IJSStackFrameInfo* frames = new IJSStackFrameInfo[count];

    JSDStackFrameInfo* jsdframe = JSD_GetStackFrame(_jsdc, jsdthreadstate);
    int i = count;
    while( jsdframe )
    {
        IScript script;
        _initScript(script, (CORBA::Long)
                    JSD_GetScriptForStackFrame(_jsdc, jsdthreadstate,jsdframe),
                    PR_TRUE);
        IJSPC pc;
        _initPC(pc, script, (CORBA::Long) 
                JSD_GetPCForStackFrame(_jsdc, jsdthreadstate, jsdframe));

        _initStackFrameInfo(frames[--i], pc, (CORBA::Long) jsdframe);

        jsdframe = JSD_GetCallingStackFrame(_jsdc, jsdthreadstate, jsdframe);
    }
    PR_ASSERT(0==i);
    sequence_of_IJSStackFrameInfo stack(count,count,frames,1);

    ThreadState* state = ThreadState::getThreadStateForCurrentThread();
    if( ! state )
    {
        PR_ASSERT(0);
        return JSD_HOOK_RETURN_CONTINUE;
    }

    if( state->isRunningHook() )
    {
        TRACE("hit a hook when this thread was already stopped. continuing..." );
        return JSD_HOOK_RETURN_CONTINUE;
    }

    state->callHook(this, hook, stack, &pcTop, jsdthreadstate);

    return state->getJSThreadState().continueState;
}    

JSDScript*     
DebugController::_findScriptAt(const char* filename, int lineno)
{
    JSDScript* script;
    JSDScript* best_script = NULL;
    JSDScript* iter = NULL;

    while( NULL != (script = JSD_IterateScripts(_jsdc, &iter)) )
    {
        if( 0 == strcmp( filename, JSD_GetScriptFilename(_jsdc,script) ) )
        {
            int first = JSD_GetScriptBaseLineNumber(_jsdc, script);
            int last = first + JSD_GetScriptLineExtent(_jsdc, script) - 1;

            if( lineno >= first && lineno <= last )
            {
                // save this as best
                best_script = script;
                // if this is a function, then we're done.
                if( NULL != JSD_GetScriptFunctionName(_jsdc, script) )
                    break;
            }
        }
    }
    return best_script;
}    

/**************************/

PR_STATIC_CALLBACK(PRHashNumber)
_hash_root(const void *key)
{
    return ((HookKey*)key)->hash_num;
}
PR_STATIC_CALLBACK(PRIntn)
_hash_key_comparer(const void *v1, const void *v2)
{
    if( ( ((HookKey*)v1)->pc.script.jsdscript == 
          ((HookKey*)v2)->pc.script.jsdscript ) &&
        ( ((HookKey*)v1)->pc.offset == 
          ((HookKey*)v2)->pc.offset ) )
        return 1;
    return 0;
}    

PR_STATIC_CALLBACK(PRIntn)
_hash_entry_zapper(PRHashEntry *he, PRIntn i, void *arg)
{
    // XXX clear the JSD_HOOK ???
    delete ((HookKey*)he->key);
    ((IJSExecutionHook_ptr)he->value)->_release();
    he->value = NULL;
    he->key   = NULL;
    return HT_ENUMERATE_NEXT;
}
    
IJSExecutionHook_ptr 
DebugController::_findInstructionHook(const IJSPC& pc)
{
    IJSExecutionHook_ptr hook = NULL; 
    if( _instructionHookTable )
    {
        _lock();
        HookKey key(pc);
        hook = (IJSExecutionHook_ptr) 
                PR_HashTableLookup(_instructionHookTable, &key);
        _unlock();
    }
    return hook;
}    

PRBool               
DebugController::_addInstructionHook(const IJSPC& pc, 
                                     IJSExecutionHook_ptr hook)
{
    PRBool retval = PR_FALSE;
    if( _instructionHookTable )
    {
        HookKey* key = new HookKey(pc);
        _lock();
        retval = PR_HashTableAdd(_instructionHookTable,key,hook)?PR_TRUE:PR_FALSE;
        _unlock();
        JSD_SetExecutionHook(_jsdc, (JSDScript*)pc.script.jsdscript, pc.offset, 
                             ssjsdExecutionHookProc, this);
    }
    return retval;
}

IJSExecutionHook_ptr
DebugController::_removeInstructionHook(const IJSPC& pc)
{
    JSD_ClearExecutionHook(_jsdc, (JSDScript*)pc.script.jsdscript, pc.offset );

    IJSExecutionHook_ptr hook = NULL; 
    if( _instructionHookTable )
    {
        HookKey key(pc);
        _lock();
        PRHashEntry** entry = PR_HashTableRawLookup(_instructionHookTable,
                                                    key.hash_num, &key );
        if( entry && *entry )
        {
            IJSExecutionHook_ptr hook = (IJSExecutionHook_ptr)(*entry)->value;
            delete ((HookKey*)(*entry)->key);
            PR_HashTableRemove(_instructionHookTable, &key );
        }
        _unlock();
    }
    return hook;
}    

PRBool               
DebugController::_initInstructionHookTable()
{
    _instructionHookTable = PR_NewHashTable( 256, _hash_root,
                                             _hash_key_comparer,
                                             PR_CompareValues,
                                             NULL, NULL);
    return _instructionHookTable ? PR_TRUE : PR_FALSE;
}

void                 
DebugController::_freeInstructionHookTable()
{
    if( _instructionHookTable )
    {
        PR_HashTableEnumerateEntries(_instructionHookTable, _hash_entry_zapper, NULL);
        PR_HashTableDestroy(_instructionHookTable);
    }
    _instructionHookTable = NULL;
}    

void 
DebugController::_clearStepHandler(PRBool canReinstate)
{
    if( canReinstate )
    {
        if( _stepHandler )
        {
            if( _oldStepHandler )
                delete _oldStepHandler;
            _oldStepHandler = _stepHandler;
            _stepHandler = NULL;
            JSD_ClearInterruptHook(_jsdc); 
        }
    }
    else
    {
        if( _stepHandler )
        {
            delete _stepHandler;
            _stepHandler = NULL;
            JSD_ClearInterruptHook(_jsdc); 
        }
        if( _oldStepHandler )
        {
            delete _oldStepHandler;
            _oldStepHandler = NULL;
        }
    }
}

void
DebugController::_reinstateStepHandler()
{
    if( _oldStepHandler && ! _stepHandler )
    {
        _stepHandler = _oldStepHandler;
        _oldStepHandler = NULL;
        JSD_SetInterruptHook(_jsdc, ssjsdExecutionHookProc, this );
    }
}    

/**************************/

HookKey::HookKey(const IJSPC& pc)
{
    this->pc = pc;
    this->hash_num = ((long)pc.script.jsdscript) + (pc.offset*7);
}

/***************************************************************************/
/***************************************************************************/

PRMonitor*   ThreadState::_monitor           = NULL;
PRHashTable* ThreadState::_thread2IndexTable = NULL;

ThreadState::ThreadState(PRThread* currentThread)
    :   _currentThread(currentThread),
        _controller(NULL),
        _hook(NULL),
        _pcTop(NULL),
        _isRunningEval(PR_FALSE),
        _isRunningHook(PR_FALSE),
        _actualCallToHookIsHappening(PR_FALSE),
        _leaveSuspended(PR_FALSE),
        _mainEventQueue(NULL),
        _hookCallerEventQueue(NULL),
        _hookCallerEventQueueCreateFinished(PR_FALSE),
        _suicideMessageReceived(PR_FALSE),
        _execResult()
{
    // do nothing...   
}

ThreadState::~ThreadState()
{
    if( _mainEventQueue )
        PR_DestroyEventQueue(_mainEventQueue);
    _removeIndexForThread(_currentThread);
}    

PRHashNumber
ThreadState::_hash_root(const void *key)
{
    PRHashNumber num = (PRHashNumber) key;  /* help lame MSVC1.5 on Win16 */
    return num >> 2;
}

PRBool 
ThreadState::_prepThread2IndexTable()
{
    if(_thread2IndexTable)
        return PR_TRUE;

    _lock();
    // re check to avoid nasty race...
    if(! _thread2IndexTable)
        _thread2IndexTable = PR_NewHashTable( 256, _hash_root,
                                              PR_CompareValues,
                                              PR_CompareValues,
                                              NULL, NULL);
    _unlock();

    PR_ASSERT(_thread2IndexTable);
    return _thread2IndexTable ? PR_TRUE : PR_FALSE;
}

PRBool 
ThreadState::_findIndexForThread(PRThread* thread, PRUintn* index)
{
    PRUintn indexPlusOne;

    PR_ASSERT(thread);
    PR_ASSERT(index);

    if( ! _prepThread2IndexTable() )
        return PR_FALSE;

    _lock();
    indexPlusOne = (PRUintn) PR_HashTableLookup(_thread2IndexTable, thread);
    _unlock();

    if(indexPlusOne)
    {
        *index = indexPlusOne - 1;
        return PR_TRUE;
    }
    return PR_FALSE;
}

PRBool 
ThreadState::_setIndexForThread(PRThread* thread, PRUintn index)
{
    PRBool retval;

    PR_ASSERT(thread);

    if( ! _prepThread2IndexTable() )
        return PR_FALSE;

    _lock();
    retval = PR_HashTableAdd(_thread2IndexTable,thread,(void*)(index+1)) ?
        PR_TRUE : PR_FALSE;
    _unlock();

    return retval;
}

PRBool 
ThreadState::_removeIndexForThread(PRThread* thread)
{
    PRBool retval;

    PR_ASSERT(thread);
    
    if( ! _prepThread2IndexTable() )
        return PR_FALSE;

    _lock();
    retval = PR_HashTableRemove(_thread2IndexTable,thread)?PR_TRUE:PR_FALSE;
    _unlock();

    return retval;
}

/**************************/

ThreadState* 
ThreadState::findThreadStateForCurrentThread(void)
{
    return _findThreadState(PR_GetCurrentThread());
}

ThreadState* 
ThreadState::getThreadStateForCurrentThread(void)
{
    PRThread* currentThread = PR_GetCurrentThread();
    ThreadState* ts = _findThreadState(currentThread);
    if(! ts)
        ts = _createThreadState(currentThread);
    return ts;
}    

ThreadState* 
ThreadState::_findThreadState(PRThread* currentThread)
{
    PRUintn index;

    PR_ASSERT(PR_GetCurrentThread() == currentThread);

    if( _findIndexForThread(currentThread, &index) )
        return (ThreadState*) PR_GetThreadPrivate(index);
    return NULL;
}

ThreadState* 
ThreadState::_createThreadState(PRThread* currentThread)
{
    PRUintn index;

    if(PR_FAILURE == PR_NewThreadPrivateIndex(&index,_callbackForPRThreadDeath))
        return NULL;

    if( ! _setIndexForThread(currentThread, index) )
        return NULL;

    ThreadState* ts = new ThreadState(currentThread);

    if( ! ts->_initEventQueues() )
        return NULL;

    if( PR_FAILURE == PR_SetThreadPrivate(index, ts) )
        return NULL;
    
    return ts;
}

/**************************/

PRBool 
ThreadState::_initEventQueues(void)
{
    char name[64];

    PR_ASSERT(NULL == _mainEventQueue);
    PR_ASSERT(NULL == _hookCallerEventQueue);
    PR_ASSERT(! _hookCallerEventQueueCreateFinished);
    PR_ASSERT(! _suicideMessageReceived);

    sprintf(name, "ThreadState::_mainEventQueue_%d", (int) this);

    _mainEventQueue = PR_CreateEventQueue(name, _currentThread);
    if( ! _mainEventQueue )
        return PR_FALSE;

    // the _hookCallerEventQueue queue gets created on the 'other' thread.

    _lock();
    if( PR_CreateThread(PR_USER_THREAD,
                        _otherThreadProc,
                        this, 
                        PR_PRIORITY_NORMAL, 
                        PR_GLOBAL_THREAD, 
                        PR_UNJOINABLE_THREAD,
                        0) )
    {
        while( ! _hookCallerEventQueueCreateFinished )
        {
            _wait();
        }
    }
    _unlock();

    return NULL == _hookCallerEventQueue ? PR_FALSE : PR_TRUE;
}    

void 
ThreadState::_otherThreadProc(void* arg)
{
    char name[64];
    ThreadState* self = (ThreadState*)arg;

    sprintf(name, "ThreadState::_hookCallerEventQueue_%d", (int) self);

    _lock();
    self->_hookCallerEventQueue = PR_CreateEventQueue(name, PR_GetCurrentThread());
    self->_hookCallerEventQueueCreateFinished = PR_TRUE;
    _notify();
    _unlock();

    // run in the queue until we process the suicide message

    if(self->_hookCallerEventQueue)
    {
        // run the queue

        while( ! self->_suicideMessageReceived )
        {
            PREvent* e = PR_WaitForEvent(self->_hookCallerEventQueue);
            if(e)
            {
                PR_HandleEvent(e);
            }
        }

        // delete our queue        
        PR_DestroyEventQueue(self->_hookCallerEventQueue);
        self->_hookCallerEventQueue = NULL;
        // delete our self!
        delete self;
    }
    // return here ends this 'other' thread
}    

void 
ThreadState::_callbackForPRThreadDeath(void *arg)
{
    ThreadState* self = (ThreadState*)arg;
    if(self->_hookCallerEventQueue)
    {
        // post suicide message
        JSDSuicideEvent* e = new JSDSuicideEvent(self);
        e->post(self->_hookCallerEventQueue);
    }
    else
    {
        // otherwise, just delete it.
        delete self;
    }
}    

/**************************/

void
ThreadState::callHook(DebugController*               controller,
                      IJSExecutionHook*              hook,
                      sequence_of_IJSStackFrameInfo& stack,
                      IJSPC*                         pcTop,
                      JSDThreadState*                jsdthreadstate)
{
    PR_ASSERT(controller);
    PR_ASSERT(hook);
    PR_ASSERT(pcTop);
    PR_ASSERT(jsdthreadstate);
    PR_ASSERT(_mainEventQueue);
    PR_ASSERT(_hookCallerEventQueue);
    PR_ASSERT(! _isRunningHook);
    PR_ASSERT(! _actualCallToHookIsHappening);

    if( ! _mainEventQueue || ! _hookCallerEventQueue )
        return;

    // setup IJSThreadState
    _jsts.stack          = stack;
    _jsts.continueState  = (CORBA::Long) JSD_HOOK_RETURN_CONTINUE;
    _jsts.returnValue    = (const char*) NULL;  // currently ignored
    _jsts.status         = 0;                   // currently ignored
    _jsts.jsdthreadstate = (CORBA::Long) jsdthreadstate;
    _jsts.id             = (CORBA::Long) this;

    _controller = controller;
    _hook       = hook;
    _pcTop      = pcTop;

    _leaveSuspended = PR_FALSE;
    _actualCallToHookIsHappening = PR_TRUE;

    _isRunningHook = PR_TRUE;

    // post hook caller event
    JSDCallHookEvent* e = new JSDCallHookEvent(this);
    e->post(_hookCallerEventQueue);

    // proccess events until a JSDResumeEvent clears _isRunningHook
    while( _isRunningHook )
    {
        PREvent* e = PR_WaitForEvent(_mainEventQueue);
        if(e)
        {
            PR_HandleEvent(e);
        }
    }
}    

PRBool  
ThreadState::isRunningHook(void)
{
    return _isRunningHook;
}

PRBool  ThreadState::isWaitingForResume(void)
{
    if(_isRunningHook && _leaveSuspended && ! _actualCallToHookIsHappening)
        return PR_TRUE;
    return PR_FALSE;
}

//
// the critical flags we _lock() to protect are:
//      _leaveSuspended
//      _actualCallToHookIsHappening
//

void    
ThreadState::leaveThreadSuspended(void)
{
    _lock();
    _leaveSuspended = PR_TRUE;
    _unlock();
}

void    
ThreadState::resumeThread()
{
    _lock();
    PR_ASSERT(_isRunningHook);
    _leaveSuspended = PR_FALSE;
    if(! _actualCallToHookIsHappening)
    {
        PR_ASSERT(_mainEventQueue);
        JSDResumeEvent* e = new JSDResumeEvent(this);
        e->post(_mainEventQueue);
    }
    _unlock();
}

void
ThreadState::eventSaysSuicide(void)
{
    _suicideMessageReceived = PR_TRUE;
}    
void
ThreadState::eventSaysResume(void)
{
    PR_ASSERT(_isRunningHook);
    _isRunningHook = PR_FALSE;
}    

// called on 'other' thread...
void 
ThreadState::eventSaysCallHook(void)
{
    PR_ASSERT(_isRunningHook);
    PR_ASSERT(_hook);

    try
    {
        TRACE("+++HOOK _hook->aboutToExecute() call hook");
        _hook->aboutToExecute(_jsts, *_pcTop);
        TRACE("---HOOK _hook->aboutToExecute() call hook");
    }
    catch(...)
    {
        // eat it...        
        _jsts.continueState = JSD_HOOK_RETURN_CONTINUE;
    }

    _lock();
    _actualCallToHookIsHappening = PR_FALSE;
    if(! _leaveSuspended)
    {
        JSDResumeEvent* e = new JSDResumeEvent(this);
        e->post(_mainEventQueue);
    }
    _unlock();
}    

/***************************************************************************/
/***************************************************************************/


JSDResumeEvent::JSDResumeEvent(ThreadState* ts)
    :   _ts(ts)
{
    PR_ASSERT(_ts);
    PR_InitEvent(this, NULL, static_handle, static_destroy);
}

void
JSDResumeEvent::post(PREventQueue* queue)
{
    PR_ASSERT(queue);
//    PR_ENTER_EVENT_QUEUE_MONITOR(queue);
    PR_PostEvent(queue, this);
//    PR_EXIT_EVENT_QUEUE_MONITOR(queue);
}    

void* 
JSDResumeEvent::static_handle(PREvent* e)
{
    JSDResumeEvent* self = (JSDResumeEvent*) e;
    PR_ASSERT(self);
    PR_ASSERT(self->_ts);
    self->_ts->eventSaysResume();
    return NULL;
}    

void
JSDResumeEvent::static_destroy(PREvent* e)
{
    JSDResumeEvent* self = (JSDResumeEvent*) e;
    PR_ASSERT(self);
    delete self;
}    

/***************************************************************************/

JSDSuicideEvent::JSDSuicideEvent(ThreadState* ts)
    :   _ts(ts)
    
{
    PR_ASSERT(_ts);
    PR_InitEvent(this, NULL, static_handle, static_destroy);
}

void
JSDSuicideEvent::post(PREventQueue* queue)
{
    PR_ASSERT(queue);
//    PR_ENTER_EVENT_QUEUE_MONITOR(queue);
    PR_PostEvent(queue, this);
//    PR_EXIT_EVENT_QUEUE_MONITOR(queue);
}    

void* 
JSDSuicideEvent::static_handle(PREvent* e)
{
    JSDSuicideEvent* self = (JSDSuicideEvent*) e;
    PR_ASSERT(self);
    PR_ASSERT(self->_ts);
    self->_ts->eventSaysSuicide();
    return NULL;
}    

void
JSDSuicideEvent::static_destroy(PREvent* e)
{
    JSDSuicideEvent* self = (JSDSuicideEvent*) e;
    PR_ASSERT(self);
    delete self;
}    

/***************************************************************************/

JSDCallHookEvent::JSDCallHookEvent(ThreadState* ts)
    :   _ts(ts)
{
    PR_ASSERT(_ts);
    PR_InitEvent(this, NULL, static_handle, static_destroy);
}

void 
JSDCallHookEvent::post(PREventQueue* queue)
{
    PR_ASSERT(queue);
//    PR_ENTER_EVENT_QUEUE_MONITOR(queue);
    PR_PostEvent(queue, this);
//    PR_EXIT_EVENT_QUEUE_MONITOR(queue);
}

void* 
JSDCallHookEvent::static_handle(PREvent* e)
{
    JSDCallHookEvent* self = (JSDCallHookEvent*) e;
    PR_ASSERT(self);
    PR_ASSERT(self->_ts);
    self->_ts->eventSaysCallHook();
    return NULL;
}

void  
JSDCallHookEvent::static_destroy(PREvent* e)
{
    JSDCallHookEvent* self = (JSDCallHookEvent*) e;
    PR_ASSERT(self);
    delete self;
}

/***************************************************************************/

JSDExecuteEvent::JSDExecuteEvent(JSDContext*              jsdc,
                                 ThreadState*             ts,
                                 const IJSStackFrameInfo& frame,
                                 const char*              text,
                                 const char*              filename,
                                 int                      lineno)
    :   _jsdc(jsdc),
        _ts(ts),
        _frame(frame),
        _text(strdup(text)),
        _filename(strdup(filename)),    // set below
        _lineno(lineno)
{
    PR_ASSERT(_jsdc);
    PR_ASSERT(_ts);
    PR_ASSERT(_text);
    PR_ASSERT(_filename);
    PR_InitEvent(this, NULL, static_handle, static_destroy);
}

JSDExecuteEvent::~JSDExecuteEvent()
{
    if(_text)
        free(_text);
    if(_filename)
        free(_filename);
}

IExecResult*
JSDExecuteEvent::postSynchronous(PREventQueue* queue)
{
    PR_ASSERT(queue);
    IExecResult* result = (IExecResult*) PR_PostSynchronousEvent(queue, this);
    return result;
}    

void* 
JSDExecuteEvent::static_handle(PREvent* e)
{
    JSDExecuteEvent* self = (JSDExecuteEvent*) e;
    PR_ASSERT(self);
    PR_ASSERT(self->_ts);
    return self->_handle();    
}

void  
JSDExecuteEvent::static_destroy(PREvent* e)
{
    JSDExecuteEvent* self = (JSDExecuteEvent*) e;
    PR_ASSERT(self);
    delete self;
}    

void* 
JSDExecuteEvent::_handle()
{
    const IJSThreadState& jsts = _ts->getJSThreadState();
    JSDThreadState* jsdthreadstate = (JSDThreadState*)(jsts.jsdthreadstate);
    JSDStackFrameInfo* jsdframe = (JSDStackFrameInfo*) _frame.jsdframe;
    JSString* jsstr;
    jsval rval;
    JSBool success;
    char* str = NULL;
    IExecResult* result = NULL;

    // clear error result
    _ts->getExecResult() = (const IExecResult&) IExecResult();

    _ts->setRunningEval(PR_TRUE);
    success = JSD_EvaluateScriptInStackFrame(_jsdc, jsdthreadstate, jsdframe, 
                                             _text, strlen(_text),
                                             _filename, _lineno, 
                                             &rval);
    _ts->setRunningEval(PR_FALSE);

    result = new IExecResult((const IExecResult&) _ts->getExecResult());

    if( success && ! JSVAL_IS_NULL(rval) && ! JSVAL_IS_VOID(rval) )
    {
        jsstr = JSD_ValToStringInStackFrame(_jsdc,jsdthreadstate,jsdframe,rval);
        if( jsstr )
        {
            str = (char*) malloc((jsstr->length+1)*sizeof(char));
            if(str)
            {
                memcpy( str, jsstr->bytes, jsstr->length*sizeof(char));
                str[jsstr->length] = 0;
            }
        }
    }
    result->result = str;
    
    return result;
}    


/***************************************************************************/

CallChain::CallChain(JSDContext* jsdc, JSDThreadState* jsdthreadstate)
    :   _jsdc(jsdc),
        _chain(NULL),
        _count(0)
{
    PR_ASSERT(_jsdc);
    PR_ASSERT(jsdthreadstate);

    _count = (int) JSD_GetCountOfStackFrames(jsdc, jsdthreadstate);

    PR_ASSERT(_count);

    _chain = (JSDScript**) new char[_count*sizeof(JSDScript*)];
    PR_ASSERT(_chain);

    JSDStackFrameInfo* jsdframe = JSD_GetStackFrame(jsdc, jsdthreadstate);

    for(int i = _count-1; i >= 0; i--)
    {
        PR_ASSERT(jsdframe);
        _chain[i] = JSD_GetScriptForStackFrame(_jsdc, jsdthreadstate, jsdframe);
        PR_ASSERT(_chain[i]);
        jsdframe = JSD_GetCallingStackFrame(jsdc, jsdthreadstate, jsdframe);
    }
}

CallChain::~CallChain()
{
    if( _chain )
        delete (char*) _chain;
}

CallChain::CompareResult 
CallChain::compare(const CallChain& other)
{
    if( this == &other )
        return EQUAL;

    if( NULL == _chain || NULL == other._chain || 0 == _count || 0 == other._count )
        return DISJOINT;

    int lesser_count = min( _count, other._count );
    for( int i = 0; i < lesser_count; i++ )
    {
        if( _chain[i] != other._chain[i] )
            return DISJOINT;
    }

    if( _count > other._count )
        return CALLER;
    if( _count < other._count )
        return CALLEE;
    return EQUAL;
}

/***************************************************************************/

StepHandler::StepHandler(JSDContext* jsdc, ThreadState* state, PRBool buildCallChain)
    :   _jsdc(jsdc), 
        topScriptInitial(NULL),
        topPCInitial(0),
        topLineInitial(0),
        _callChain(NULL)
{
    PR_ASSERT(_jsdc);
    PR_ASSERT(state);
    PR_ASSERT(state->isRunningHook());

    JSDThreadState* jsdthreadstate = (JSDThreadState*)
                            state->getJSThreadState().jsdthreadstate;
    PR_ASSERT(jsdthreadstate);

    JSDStackFrameInfo* jsdframe = _topFrame(jsdthreadstate);
    PR_ASSERT(jsdframe);

    topScriptInitial = _topScript(jsdthreadstate, jsdframe);
    PR_ASSERT(topScriptInitial);

    topPCInitial = _topPC(jsdthreadstate, jsdframe);
    topLineInitial = _topLine(topScriptInitial, topPCInitial);

    if( buildCallChain )
    {
        _callChain = new CallChain( _jsdc, jsdthreadstate );
        PR_ASSERT(_callChain);
    }
}    

StepHandler::~StepHandler()
{
    if( _callChain )
        delete  _callChain;
}    

/***************************************************************************/

StepInto::StepInto(JSDContext* jsdc, ThreadState* state)
    :   StepHandler(jsdc, state, PR_FALSE)
{
    // do nothing
}

StepHandler::StepResult 
StepInto::step(JSDThreadState* jsdthreadstate)
{
    JSDStackFrameInfo* topFrame  = _topFrame(jsdthreadstate);
    JSDScript*         topScript = _topScript(jsdthreadstate, topFrame);

    // different script, gotta stop
    if( topScriptInitial != topScript )
        return STOP;

    //
    // NOTE: This little dance is necessary because line numbers for PCs 
    // are not always acending. e,g, in:
    //
    //  for(i=0;i<count;i++)
    //     doSomething();
    //
    // The "i++" comes after the "doSomething" call as far as PCs go.
    // But, in JS the line number associated with the "i++" instruction
    // is that of the "for(...". So, the lines for the PC can look like
    // 1,1,2,1,2. Thus we are careful in deciding how to step.
    //

    prword_t    topPC   = _topPC(jsdthreadstate, topFrame);
    int         topLine = _topLine(topScript, topPC);

    // definitely jumping back
    if( topPC < topPCInitial && topLine != topLineInitial )
        return STOP;

    // definitely jumping forward
    if( topLine > topLineInitial )
        return STOP;

    return CONTINUE_SEND_INTERRUPT;
}

/***************************************************************************/

StepOver::StepOver(JSDContext* jsdc, ThreadState* state)
    :   StepHandler(jsdc, state, PR_TRUE)
{
    // do nothing
}

StepHandler::StepResult 
StepOver::step(JSDThreadState* jsdthreadstate)
{
    switch( _callChain->compare( CallChain(_jsdc, jsdthreadstate) ) )
    {
        case CallChain::EQUAL:
            break;
        case CallChain::CALLER:
            return STOP;
        case CallChain::CALLEE:
            return CONTINUE_SEND_INTERRUPT;
        case CallChain::DISJOINT:
            return STOP;
        default:
            PR_ASSERT(0);   // illegal value!
            break;
    }

    JSDStackFrameInfo* topFrame  = _topFrame(jsdthreadstate);
    JSDScript*         topScript = _topScript(jsdthreadstate, topFrame);
    prword_t           topPC   = _topPC(jsdthreadstate, topFrame);
    int                topLine = _topLine(topScript, topPC);

    // definitely jumping back
    if( topPC < topPCInitial && topLine != topLineInitial )
        return STOP;

    // definitely jumping forward
    if( topLine > topLineInitial )
        return STOP;

    return CONTINUE_SEND_INTERRUPT;
}

/***************************************************************************/

StepOut::StepOut(JSDContext* jsdc, ThreadState* state)
    :   StepHandler(jsdc, state, PR_TRUE)
{
    // do nothing
}

StepHandler::StepResult 
StepOut::step(JSDThreadState* jsdthreadstate)
{
    switch( _callChain->compare( CallChain(_jsdc, jsdthreadstate) ) )
    {
        case CallChain::EQUAL:
            return CONTINUE_SEND_INTERRUPT;
        case CallChain::CALLER:
            return STOP;
        case CallChain::CALLEE:
            return CONTINUE_SEND_INTERRUPT;
        case CallChain::DISJOINT:
            return STOP;
        default:
            PR_ASSERT(0);   // illegal value!
            return STOP;
    }
}    

/***************************************************************************/
/***************************************************************************/

SourceTextProvider::SourceTextProvider(const char* name)
    :   _sk_ISourceTextProvider(name),
        _jsdc(NULL)

{
    // do nothing
}    

SourceTextProvider::SourceTextProvider(const char* name, JSDContext* jsdc)
    :   _sk_ISourceTextProvider(name),
        _jsdc(NULL)
{
    init(jsdc);
}    

void
SourceTextProvider::init(JSDContext* jsdc)
{
    _jsdc = jsdc;
    _forcePreLoad();
}    

SourceTextProvider::~SourceTextProvider()
{
}    

ISourceTextProvider::sequence_of_string * 
SourceTextProvider::getAllPages()
{
    TRACE("SourceTextProvider::getAllPages() called");
    JSDSourceText* jsdsrc;
    JSDSourceText* iter;
    ISourceTextProvider::sequence_of_string * seq = NULL;

    _lock();

    // iterate once to get count;
    int count = 0;
    iter = NULL;
    while( NULL != JSD_IterateSources(_jsdc, &iter) )
        count++;

    char** pp = (char**) malloc(count * sizeof(void*));
    if(pp)
    {
        int i = 0;
        iter = NULL;
        while( NULL != (jsdsrc = JSD_IterateSources(_jsdc, &iter)) )
        {
            const char* url;
            if( NULL == (url = JSD_GetSourceURL(_jsdc, jsdsrc)) )
                continue;
            
            pp[i++] = strdup(url);
        }
        seq = new ISourceTextProvider::sequence_of_string(i, i, pp, 1);
    }

    _unlock();

    return seq;
}

void 
SourceTextProvider::refreshAllPages()
{
    TRACE("SourceTextProvider::refreshAllPages() called");
    _lock();
    // what we really want is a way to detect if existing pages
    // have changed - date, size, etc... -- and force reload of them
//    JSD_DestroyAllSources(_jsdc);
    _forcePreLoad();
    _unlock();
}    

CORBA::Boolean 
SourceTextProvider::hasPage(const char * arg0)
{
    TRACE("SourceTextProvider::hasPage() called");
    return JSD_FindSourceForURL(_jsdc, arg0) ? 1 : 0;
        
}    

CORBA::Boolean 
SourceTextProvider::loadPage(const char * arg0)
{
    TRACE("SourceTextProvider::loadPage() called");
    // not implemented...
    return 0;    
}    

void 
SourceTextProvider::refreshPage(const char * arg0)
{
    TRACE("SourceTextProvider::refreshPage() called");
    // not implemented...
}    

char * 
SourceTextProvider::getPageText(const char * arg0)
{
    TRACE("SourceTextProvider::getPageText() called");
    JSDSourceText* jsdsrc;
    const char *   const_text;
    int            len;
    char*          text = NULL;

    _lock();

    jsdsrc = JSD_FindSourceForURL(_jsdc, arg0);
    if( jsdsrc )
    {
        if( JSD_SOURCE_COMPLETED != JSD_GetSourceStatus(_jsdc, jsdsrc) )
            jsdsrc = JSDLW_ForceLoadSource(_jsdc, jsdsrc);
    }

    if( jsdsrc )
    {
        if( JSD_GetSourceText(_jsdc, jsdsrc, &const_text, &len) && 
            const_text && len )
        {
            text = (char*) malloc((len+1)*sizeof(char));
            if(text)
            {
                memcpy(text,const_text,len*sizeof(char));
                text[len] = 0;
                _deferedFree(text);
            }
        }
    }

    _unlock();

    return text;
}    

CORBA::Long 
SourceTextProvider::getPageStatus(const char * arg0)
{
    TRACE("SourceTextProvider::getPageStatus() called");
    JSDSourceText* jsdsrc;
    int status = JSD_SOURCE_INITED;

    _lock();
    jsdsrc = JSD_FindSourceForURL(_jsdc, arg0);
    if(jsdsrc)
        status = JSD_GetSourceStatus(_jsdc, jsdsrc);
    _unlock();
    return (CORBA::Long) status;
}    

CORBA::Long 
SourceTextProvider::getPageAlterCount(const char * arg0)
{
    TRACE("SourceTextProvider::getPageAlterCount() called");
    JSDSourceText* jsdsrc;
    int altercount = JSD_SOURCE_INITED;

    _lock();
    jsdsrc = JSD_FindSourceForURL(_jsdc, arg0);
    if(jsdsrc)
        altercount = JSD_GetSourceAlterCount(_jsdc, jsdsrc);
    _unlock();
    return (CORBA::Long) altercount;
}    

void 
SourceTextProvider::_forcePreLoad()
{
    LWDBGApp* app;
    LWDBGApp* app_iter;

    _lock();

    // iterate through all pages of all apps to force preload
    app_iter = NULL;
    while(NULL != (app = LWDBG_EnumerateApps(&app_iter)))
    {
        const char* pagename;
        void* page_iter = NULL;
        while(NULL != (pagename = LWDBG_EnumeratePageNames(app,&page_iter)))
        {
            JSDLW_PreLoadSource(_jsdc, app, pagename, JS_FALSE );
        }
    }
    _unlock();
}    

/***************************************************************************/

char * 
TestInterface_impl::getFirstAppInList()
{
    static char buf[1024];

    LWDBGApp* iter = NULL;
    LWDBGApp* app;

    app = LWDBG_EnumerateApps(&iter);
    if( app )
        return ((char*) LWDBG_GetURI(app)) + 1;
    return NULL;        
}

void 
TestInterface_impl::getAppNames(StringReciever_ptr sr)
{
    LWDBGApp* iter = NULL;
    LWDBGApp* app;
    
    while( NULL != (app = LWDBG_EnumerateApps(&iter)) )
    {
        char* name = ((char*) LWDBG_GetURI(app)) + 1;

        try
        {
            sr->recieveString(name);
        }
        catch(const CORBA::Exception&)
        {
            // eat it and bail...
            return;
        }
    }
}

static void
InitThing(Thing& t, const char* s, int i)
{
    t.s = s;
    t.i = i;
}

TestInterface::sequence_of_Thing * 
TestInterface_impl::getThings()
{
    Thing* p = new Thing[3];
    InitThing(p[0],"zero",0);
    InitThing(p[1],"one" ,1);
    InitThing(p[2],"two" ,2);
    return new TestInterface::sequence_of_Thing(3, 3, p, 1);
}    

void 
TestInterface_impl::callBounce(StringReciever_ptr arg0, CORBA::Long arg1)
{
        try
        {
            arg0->bounce(arg1);
        }
        catch(const CORBA::Exception&)
        {
            // eat it and bail...
        }
}    


/***************************************************************************/
/***************************************************************************/

static JSDContext*          _jsdc = NULL;
static TestInterface_impl   _test;
static SourceTextProvider   _stp("JSSourceTextProvider");
static DebugController      _controller("JSDebugController");
static char*                _hostname;

static const char*
_WAIReturnType2String( WAIReturnType_t t )
{
    switch(t)
    {
        case WAISPISuccess:       return "WAISPISuccess";
        case WAISPIFailure:       return "WAISPIFailure";
        case WAISPIBadparam:      return "WAISPIBadparam";
        case WAISPINonameservice: return "WAISPINonameservice";
        default:                  return "unknown error code";
    }
}    

// PR_STATIC_CALLBACK(void)
// ssjsdTestThreadProc(void* arg)
// {
//     CORBA::ORB_var orb;
//     CORBA::BOA_var boa;
// 
//     int zero = 0;
//     orb = CORBA::ORB_init(zero,NULL);
//     boa = orb->BOA_init(zero,NULL);
// 
//     boa->obj_is_ready(&_test);
//     WAIReturnType_t reg = registerObject(_hostname, "test", &_test );
// 
//     log_error(LOG_VERBOSE, "ssjsdTestThreadProc", NULL, NULL, 
//               "registration of _test %s", 
//               _WAIReturnType2String(reg) );
// 
//     boa->impl_is_ready();
// }    
// 
// PR_STATIC_CALLBACK(void)
// ssjsdDebugControllerThreadProc(void* arg)
// {
//     CORBA::ORB_var orb;
//     CORBA::BOA_var boa;
// 
//     int zero = 0;
//     orb = CORBA::ORB_init(zero,NULL);
//     boa = orb->BOA_init(zero,NULL);
// 
//     _controller.init(_jsdc);
//     boa->obj_is_ready(&_controller);
//     WAIReturnType_t reg = registerObject(_hostname, "JSDebugController", &_controller );
// 
//     log_error(LOG_VERBOSE, "ssjsdDebugControllerThreadProc", NULL, NULL, 
//           "registration of DebugController %s", 
//           _WAIReturnType2String(reg) );
// 
//     boa->impl_is_ready();
// }    
// 
// PR_STATIC_CALLBACK(void)
// ssjsdSourceTextProviderThreadProc(void* arg)
// {
//     CORBA::ORB_var orb;
//     CORBA::BOA_var boa;
// 
//     int zero = 0;
//     orb = CORBA::ORB_init(zero,NULL);
//     boa = orb->BOA_init(zero,NULL);
// 
//     _stp.init(_jsdc);
//     boa->obj_is_ready(&_stp);
//     WAIReturnType_t reg = registerObject(_hostname, "JSSourceTextProvider", &_stp );
// 
//     log_error(LOG_VERBOSE, "ssjsdSourceTextProviderThreadProc", NULL, NULL, 
//           "registration of SourceTextProvider %s", 
//           _WAIReturnType2String(reg) );
//     boa->impl_is_ready();
// }    
// 
// static PRThread*            _TestThread;
// static PRThread*            _DebugControllerThread;
// static PRThread*            _SourceTextProviderThread;
// 
// static void
// _exportRemotedObjectsOnSeparateThreads()
// {
//     ///////////////////////////
// 
//     _TestThread =
//         PR_CreateThread( PR_USER_THREAD,
//                          ssjsdTestThreadProc,
//                          NULL,
//                          PR_PRIORITY_NORMAL,
//                          PR_GLOBAL_THREAD,
//                          PR_UNJOINABLE_THREAD,
//                          0 );
// 
//     log_error(LOG_VERBOSE, "ssjsdebugInit", sn, rq, 
//             "create test thread %s", 
//             _TestThread == NULL ? "failed" : "succeeded" );
// 
//     ///////////////////////////
// 
//     _DebugControllerThread =
//         PR_CreateThread( PR_USER_THREAD,
//                          ssjsdDebugControllerThreadProc,
//                          NULL,
//                          PR_PRIORITY_NORMAL,
//                          PR_GLOBAL_THREAD,
//                          PR_UNJOINABLE_THREAD,
//                          0 );
// 
//     log_error(LOG_VERBOSE, "ssjsdebugInit", sn, rq, 
//             "create debug controller thread %s", 
//             _DebugControllerThread == NULL ? "failed" : "succeeded" );
// 
// 
//     ///////////////////////////
// 
//     _SourceTextProviderThread =
//         PR_CreateThread( PR_USER_THREAD,
//                          ssjsdSourceTextProviderThreadProc,
//                          NULL,
//                          PR_PRIORITY_NORMAL,
//                          PR_GLOBAL_THREAD,
//                          PR_UNJOINABLE_THREAD,
//                          0 );
// 
//     log_error(LOG_VERBOSE, "ssjsdebugInit", sn, rq, 
//             "create text provider thread %s", 
//             _SourceTextProviderThread == NULL ? "failed" : "succeeded" );
// }    


static PRBool
_registerOb(const char *name, CORBA::Object_ptr obj)
{
    WAIReturnType_t reg = registerObject(_hostname, name, obj);

    log_error(LOG_VERBOSE, "ssjsdebugInit", NULL, NULL, 
              "registration of %s returned %s", 
              name, _WAIReturnType2String(reg));

   return WAISPISuccess == reg ? PR_TRUE : PR_FALSE;
}    

static void
_exportRemotedObjectsOnThisThread()
{
    CORBA::ORB_var orb;
    CORBA::BOA_var boa;

    int zero = 0;
    orb = CORBA::ORB_init(zero,NULL);
    boa = orb->BOA_init(zero,NULL);

    ///////////////////////////

//    boa->obj_is_ready(&_test);
//    _registerOb("test", &_test, force );

    ///////////////////////////

    _controller.init(_jsdc);
    boa->obj_is_ready(&_controller);
    _registerOb("JSDebugController", &_controller);

    ///////////////////////////

    _stp.init(_jsdc);
    boa->obj_is_ready(&_stp);
    _registerOb("JSSourceTextProvider", &_stp);
}



/* NSAPI initialization function */

PR_IMPLEMENT(int)
ssjsdebugInit(pblock *pb, Session *sn, Request *rq)
{
    JSD_SetUserCallbacks(LWDBG_GetTaskState(), NULL, NULL);
    _jsdc = JSD_DebuggerOn();

    log_error(LOG_VERBOSE, "ssjsdebugInit", NULL, NULL, 
              "starting with iiop, JSD_DebuggerOn() %s", _jsdc == NULL ? "failed" : "succeeded" );

    if(_jsdc)
    {
        _hostname = http_uri2url("","");

        log_error(LOG_VERBOSE, "ssjsdebugInit", NULL, NULL, 
              "hostname = %s", _hostname );

        _exportRemotedObjectsOnThisThread();
    }
    return REQ_PROCEED;
}    
