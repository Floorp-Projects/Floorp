/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_localstorage_LocalStorageCommon_h
#define mozilla_dom_localstorage_LocalStorageCommon_h

#include <cstdint>
#include "ErrorList.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/quota/QuotaCommon.h"
#include "nsLiteralString.h"
#include "nsStringFwd.h"

/*
 * Local storage
 * ~~~~~~~~~~~~~
 *
 * Implementation overview
 * ~~~~~~~~~~~~~~~~~~~~~~~
 *
 * The implementation is based on a per principal/origin cache (datastore)
 * living in the main process and synchronous calls initiated from content
 * processes.
 * The IPC communication is managed by database actors which link to the
 * datastore.
 * The synchronous blocking of the main thread is done by using a special
 * technique or by using standard synchronous IPC calls.
 *
 * General architecture
 * ~~~~~~~~~~~~~~~~~~~~
 * The current browser architecture consists of one main process and multiple
 * content processes (there are other processes but for simplicity's sake, they
 * are not mentioned here). The processes use the IPC communication to talk to
 * each other. Local storage implementation uses the client-server model, so
 * the main process manages all the data and content processes then request
 * particular data from the main process. The main process is also called the
 * parent or the parent side, the content process is then called the child or
 * the child side.
 *
 * Datastores
 * ~~~~~~~~~~
 *
 * A datastore provides a convenient way to access data for given origin. The
 * data is always preloaded into memory and indexed using a hash table. This
 * enables very fast access to particular stored items. There can be only one
 * datastore per origin and exists solely on the parent side. It is represented
 * by the "Datastore" class. A datastore instance is a ref counted object and
 * lives on the PBackground thread, it is kept alive by database objects. When
 * the last database object for given origin is destroyed, the associated
 * datastore object is destroyed too.
 *
 * Databases
 * ~~~~~~~~~
 *
 * A database allows direct access to a datastore from a content process. There
 * can be multiple databases for the same origin, but they all share the same
 * datastore.
 * Databases use the PBackgroundLSDatabase IPDL protocol for IPC communication.
 * Given the nature of local storage, most of PBackgroundLSDatabase messages
 * are synchronous.
 *
 * On the parent side, the database is represented by the "Database" class that
 * is a parent actor as well (implements the "PBackgroundLSDatabaseParent"
 * interface). A database instance is a ref counted object and lives on the
 * PBackground thread.
 * All live database actors are tracked in an array.
 *
 * On the child side, the database is represented by the "LSDatabase" class
 * that provides indirect access to a child actor. An LSDatabase instance is a
 * ref counted object and lives on the main thread.
 * The actual child actor is represented by the "LSDatabaseChild" class that
 * implements the "PBackgroundLSDatabaseChild" interface. An "LSDatabaseChild"
 * instance is not ref counted and lives on the main thread too.
 *
 * Synchronous blocking
 * ~~~~~~~~~~~~~~~~~~~~
 *
 * Local storage is synchronous in nature which means the execution can't move
 * forward until there's a reply for given method call.
 * Since we have to use IPC anyway, we could just always use synchronous IPC
 * messages for all local storage method calls. Well, there's a problem with
 * that approach.
 * If the main process needs to do some off PBackground thread stuff like
 * getting info from principals on the main thread or some asynchronous stuff
 * like directory locking before sending a reply to a synchronous message, then
 * we would have to block the thread or spin the event loop which is usually a
 * bad idea, especially in the main process.
 * Instead, we can use a special thread in the content process called
 * RemoteLazyInputStream thread for communication with the main process using
 * asynchronous messages and synchronously block the main thread until the DOM
 * File thread is done (the main thread blocking is a bit more complicated, see
 * the comment in RequestHelper::StartAndReturnResponse for more details).
 * Anyway, the extra hop to the RemoteLazyInputStream thread brings another
 * overhead and latency. The final solution is to use a combination of the
 * special thread for complex stuff like datastore preparation and synchronous
 * IPC messages sent directly from the main thread for database access when data
 * is already loaded from disk into memory.
 *
 * Requests
 * ~~~~~~~~
 *
 * Requests are used to handle asynchronous high level datastore operations
 * which are initiated in a content process and then processed in the parent
 * process (for example, preparation of a datastore).
 * Requests use the "PBackgroundLSRequest" IPDL protocol for IPC communication.
 *
 * On the parent side, the request is represented by the "LSRequestBase" class
 * that is a parent actor as well (implements the "PBackgroundLSRequestParent"
 * interface). It's an abstract class (contains pure virtual functions) so it
 * can't be used to create instances.
 * It also inherits from the "DatastoreOperationBase" class which is a generic
 * base class for all datastore operations. The "DatastoreOperationsBase" class
 * inherits from the "Runnable" class, so derived class instances are ref
 * counted, can be dispatched to multiple threads and thus they are used on
 * multiple threads. However, derived class instances can be created on the
 * PBackground thread only.
 *
 * On the child side, the request is represented by the "RequestHelper" class
 * that covers all the complexity needed to start a new request, handle
 * responses and do safe main thread blocking at the same time.
 * It inherits from the "Runnable" class, so instances are ref counted and
 * they are internally used on multiple threads (specifically on the main
 * thread and on the RemoteLazyInputStream thread). Anyway, users should create
 * and use instances of this class only on the main thread (apart from a special
 * case when we need to cancel the request from an internal chromium IPC thread
 * to prevent a dead lock involving CPOWs).
 * The actual child actor is represented by the "LSRequestChild" class that
 * implements the "PBackgroundLSRequestChild" interface. An "LSRequestChild"
 * instance is not ref counted and lives on the RemoteLazyInputStream thread.
 * Request responses are passed using the "LSRequestChildCallback" interface.
 *
 * Preparation of a datastore
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * The datastore preparation is needed to make sure a datastore is fully loaded
 * into memory. Every datastore preparation produces a unique id (even if the
 * datastore for given origin already exists).
 * On the parent side, the preparation is handled by the "PrepareDatastoreOp"
 * class which inherits from the "LSRequestBase" class. The preparation process
 * on the parent side is quite complicated, it happens sequentially on multiple
 * threads and is managed by a state machine.
 * On the child side, the preparation is done in the LSObject::EnsureDatabase
 * method using the "RequestHelper" class. The method starts a new preparation
 * request and obtains a unique id produced by the parent (or an error code if
 * the requested failed to complete).
 *
 * Linking databases to a datastore
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * A datastore exists only on the parent side, but it can be accessed from the
 * content via database actors. Database actors are initiated on the child side
 * and they need to be linked to a datastore on the parent side via an id. The
 * datastore preparation process gives us the required id.
 * The linking is initiated on the child side in the LSObject::EnsureDatabase
 * method by calling SendPBackgroundLSDatabaseConstructor and finished in
 * RecvPBackgroundLSDatabaseConstructor on the parent side.
 *
 * Actor migration
 * ~~~~~~~~~~~~~~~
 *
 * In theory, the datastore preparation request could return a database actor
 * directly (instead of returning an id intended for database linking to a
 * datastore). However, as it was explained above, the preparation must be done
 * on the RemoteLazyInputStream thread and database objects are used on the main
 * thread. The returned actor would have to be migrated from the
 * RemoteLazyInputStream thread to the main thread and that's something which
 * our IPDL doesn't support yet.
 *
 * Exposing local storage
 * ~~~~~~~~~~~~~~~~~~~~~~
 *
 * The implementation is exposed to the DOM via window.localStorage attribute.
 * Local storage's sibling, session storage shares the same WebIDL interface
 * for exposing it to web content, therefore there's an abstract class called
 * "Storage" that handles some of the common DOM bindings stuff. Local storage
 * specific functionality is defined in the "LSObject" derived class.
 * The "LSObject" class is also a starting point for the datastore preparation
 * and database linking.
 *
 * Local storage manager
 * ~~~~~~~~~~~~~~~~~~~~~
 *
 * The local storage manager exposes some of the features that need to be
 * available only in the chrome code or tests. The manager is represented by
 * the "LocalStorageManager2" class that implements the "nsIDOMStorageManager"
 * interface.
 */

// XXX Replace all uses by the QM_* variants and remove these aliases
#define LS_TRY QM_TRY
#define LS_TRY_UNWRAP QM_TRY_UNWRAP
#define LS_TRY_INSPECT QM_TRY_INSPECT
#define LS_TRY_RETURN QM_TRY_RETURN
#define LS_FAIL QM_FAIL

namespace mozilla {

class LogModule;

namespace ipc {

class PrincipalInfo;

}  // namespace ipc

namespace dom {

extern const char16_t* kLocalStorageType;

/**
 * Convenience data-structure to make it easier to track whether a value has
 * changed and what its previous value was for notification purposes.  Instances
 * are created on the stack by LSObject and passed to LSDatabase which in turn
 * passes them onto LSSnapshot for final updating/population.  LSObject then
 * generates an event, if appropriate.
 */
class MOZ_STACK_CLASS LSNotifyInfo {
  bool mChanged;
  nsString mOldValue;

 public:
  LSNotifyInfo() : mChanged(false) {}

  bool changed() const { return mChanged; }

  bool& changed() { return mChanged; }

  const nsString& oldValue() const { return mOldValue; }

  nsString& oldValue() { return mOldValue; }
};

/**
 * A check of LSNG being enabled, the value is latched once initialized so
 * changing the preference during runtime has no effect.
 * May be called on any thread in the parent process, but you should call
 * CachedNextGenLocalStorageEnabled if you know that NextGenLocalStorageEnabled
 * was already called because it is faster.
 * May be called on the main thread only in a content process.
 */
bool NextGenLocalStorageEnabled();

/**
 * Cached any-thread version of NextGenLocalStorageEnabled().
 */
bool CachedNextGenLocalStorageEnabled();

/**
 * Returns a success value containing a pair of origin attribute suffix and
 * origin key.
 */
Result<std::pair<nsCString, nsCString>, nsresult> GenerateOriginKey2(
    const mozilla::ipc::PrincipalInfo& aPrincipalInfo);

LogModule* GetLocalStorageLogger();

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_localstorage_LocalStorageCommon_h
