/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

/* 
   Progress.h -- class definitions for FE IMAP upgrade dialogs
   Created: Daniel Malmer <malmer@netscape.com>, 5-Apr-98.
 */

#ifndef _xfe_progress_h
#define _xfe_progress_h

#include "DownloadFrame.h"
#include "pw_public.h"

// work around clash with XFE_Progress in xfe.h
#define XFE_Progress XFE_Progress1

class XFE_Progress : public XFE_DownloadFrame
{
 private:
	int32 m_progress_bar_min;
	int32 m_progress_bar_max;
	PW_CancelCallback m_cancel_cb;
	void* m_cancel_closure;
    
 public:
    XFE_Progress(Widget parent);
    virtual ~XFE_Progress();

	virtual XP_Bool isCommandEnabled(CommandType cmd, void *calldata = NULL,
						XFE_CommandInfo* = NULL);
	virtual void doCommand(CommandType cmd, void *calldata = NULL,
						XFE_CommandInfo* = NULL);
	virtual XP_Bool handlesCommand(CommandType cmd, void *calldata = NULL,
						XFE_CommandInfo* = NULL);

    // C++ implementation of C API
    pw_ptr Create(MWContext* parent, PW_WindowType type);
    void SetCancelCallback(PW_CancelCallback cancelcb, void* cancelClosure);
	void Destroy(void);

    void SetWindowTitle(const char* title);
    void SetLine1(const char* text);
    void SetLine2(const char* text);

    void SetProgressText(const char* text);
    void SetProgressRange(int32 minimum, int32 maximum);
    void SetProgressValue(int32 value);
    void AssociateWindowWithContext(MWContext *context);
};

#endif


