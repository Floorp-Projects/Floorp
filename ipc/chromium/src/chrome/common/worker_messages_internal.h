// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string16.h"
#include "chrome/common/ipc_message_macros.h"
#include "googleurl/src/gurl.h"


//-----------------------------------------------------------------------------
// WorkerProcess messages
// These are messages sent from the browser to the worker process.
IPC_BEGIN_MESSAGES(WorkerProcess)
  IPC_MESSAGE_CONTROL2(WorkerProcessMsg_CreateWorker,
                       GURL  /* url */,
                       int  /* route_id */)
IPC_END_MESSAGES(WorkerProcess)


//-----------------------------------------------------------------------------
// WorkerProcessHost messages
// These are messages sent from the worker process to the browser process.

// No messages being sent in this direction for now.
//IPC_BEGIN_MESSAGES(WorkerProcessHost)
//IPC_END_MESSAGES(WorkerProcessHost)

//-----------------------------------------------------------------------------
// Worker messages
// These are messages sent from the renderer process to the worker process.
IPC_BEGIN_MESSAGES(Worker)
  IPC_MESSAGE_ROUTED3(WorkerMsg_StartWorkerContext,
                      GURL /* url */,
                      string16  /* user_agent */,
                      string16  /* source_code */)

  IPC_MESSAGE_ROUTED0(WorkerMsg_TerminateWorkerContext)

  IPC_MESSAGE_ROUTED1(WorkerMsg_PostMessageToWorkerContext,
                      string16  /* message */)

  IPC_MESSAGE_ROUTED0(WorkerMsg_WorkerObjectDestroyed)
IPC_END_MESSAGES(Worker)


//-----------------------------------------------------------------------------
// WorkerHost messages
// These are messages sent from the worker process to the renderer process.
IPC_BEGIN_MESSAGES(WorkerHost)
  IPC_MESSAGE_ROUTED1(WorkerHostMsg_PostMessageToWorkerObject,
                      string16  /* message */)

  IPC_MESSAGE_ROUTED3(WorkerHostMsg_PostExceptionToWorkerObject,
                      string16  /* error_message */,
                      int  /* line_number */,
                      string16  /* source_url*/)

  IPC_MESSAGE_ROUTED6(WorkerHostMsg_PostConsoleMessageToWorkerObject,
                      int  /* destination */,
                      int  /* source */,
                      int  /* level */,
                      string16  /* message */,
                      int  /* line_number */,
                      string16  /* source_url */)

  IPC_MESSAGE_ROUTED1(WorkerHostMsg_ConfirmMessageFromWorkerObject,
                      bool /* bool has_pending_activity */)

  IPC_MESSAGE_ROUTED1(WorkerHostMsg_ReportPendingActivity,
                      bool /* bool has_pending_activity */)

  IPC_MESSAGE_ROUTED0(WorkerHostMsg_WorkerContextDestroyed)
IPC_END_MESSAGES(WorkerHost)
