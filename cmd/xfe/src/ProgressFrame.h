/* -*- Mode: C++; tab-width: 4 -*-
   ProgressFrame.h -- class definitions for FE IMAP upgrade dialogs
   Copyright © 1998 Netscape Communications Corporation, all rights reserved.
   Created: Daniel Malmer <malmer@netscape.com>, 5-Apr-98.
 */

#ifndef _xfe_progress_frame_h
#define _xfe_progress_frame_h

#include "DownloadFrame.h"
#include "pw_public.h"

class XFE_ProgressFrame : public XFE_DownloadFrame
{
 private:
	int32 m_progress_bar_min;
	int32 m_progress_bar_max;
	PW_CancelCallback m_cancel_cb;
	void* m_cancel_closure;
    
 public:
    XFE_ProgressFrame(Widget parent);
    virtual ~XFE_ProgressFrame();

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


