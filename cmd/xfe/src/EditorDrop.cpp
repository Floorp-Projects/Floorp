/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
/* 
   EditorDrop.cpp -- class definition for the editor drop class
   Created: Alastair Gourlay <sgidev@netscape.com>, 1-Jan-96.
 */



#include "EditorDrop.h"
#include "xeditor.h"
#include "edt.h"

#if defined(DEBUG_sgidev) || defined(DEBUG_djw)
#define XDEBUG(x) x
#else
#define XDEBUG(x)
#endif

//
// XFE_EditorDrop class
//

// constructor

XFE_EditorDrop::XFE_EditorDrop(Widget parent,XFE_EditorView *editorView)
    : XFE_DropNetscape(parent)
{
    _editorView=editorView;
}

XFE_EditorDrop::~XFE_EditorDrop()
{
}

void XFE_EditorDrop::targets()
{
    _numTargets=2;
    _targets=new Atom[_numTargets];

    _targets[0]=_XA_NETSCAPE_URL;
    _targets[1]=XA_STRING;

    acceptFileTargets();
}

void XFE_EditorDrop::operations()
{
    // always copy - move/link irrelevant
    _operations=(unsigned int)XmDROP_COPY;
}


Atom XFE_EditorDrop::acceptDrop(unsigned int dropOperation,Atom *dropTargets,unsigned int numDropTargets)
{
    // reject a drag from the same browser window
    if (XFE_DragBase::_activeDragShell) {
        Widget shell=_widget;
        while (!XtIsShell(shell)) shell=XtParent(shell);
        if (shell==XFE_DragBase::_activeDragShell)
            return None;
    }
    
    // continue with regular target selection
    return XFE_DropNetscape::acceptDrop(dropOperation,dropTargets,numDropTargets);
}


int XFE_EditorDrop::processTargets(Atom *targets,const char **data,int numItems)
{
    if (!targets || !data || numItems==0)
        return FALSE;

    // process dropped files/documents

    int anySuccess=FALSE;
    for (int i=0;i<numItems;i++) {
        if (targets[i]==None || data[i]==NULL || strlen(data[i])==0)
            continue;

        // a file is just a file.. (XFE_DropNetscape parse any NetscapeURL
        // or WebJumper shortcut files in _NETSCAPE_URL)
        if (targets[i]==_XA_FILE_NAME) {
            anySuccess |= insertFile(data[i]);
        }

        // a link is just a link, unless it's a file: url, in which case
        // lets import it as a file. This covers local files, viewed in
        // Navigator, then dragged to the Editor.
        else if (targets[i]==_XA_NETSCAPE_URL) {
            XFE_URLDesktopType urlData(data[i]);
            for (int j=0;j<urlData.numItems();j++) {
                if (XP_STRNCASECMP(urlData.url(j),"file:",5)==0) {
                    anySuccess |= insertFile(urlData.url(j)+5);
                }
                else {
                    anySuccess |= insertLink(urlData.url(j));
                }
            }
        }

        // A string is just a string, unless it's a URL, which is just
        // a URL, unless it's a file: url. See above for thrilling details.
        else if (targets[i]==XA_STRING) {
            if (NET_URL_Type(data[i])!=0) {
                if (XP_STRNCASECMP(data[i],"file:",5)==0) {
                    anySuccess |= insertFile(data[i]+5);
                }
                else {
                    anySuccess |= insertLink(data[i]);
                }
            }
            else {
                anySuccess |= insertText(data[i]);
            }
        }
    }

    // if any of the dropped items succeeded, let's celebrate.
    return anySuccess;
}

int XFE_EditorDrop::insertLink(const char *data)
{
    XDEBUG(printf("XFE_EditorDrop:insertLink(%s) at (%d,%d)\n",
                  data,_dropEventX,_dropEventY));

	MWContext* context = _editorView->getContext();

	int32 x = _dropEventX + CONTEXT_DATA(context)->document_x;
	int32 y = _dropEventY + CONTEXT_DATA(context)->document_y;

	EDT_PositionCaret(context, x, y);

	fe_EditorHrefInsert(context, NULL, (char*)data);

    return TRUE;
}

int XFE_EditorDrop::insertFile(const char *data)
{
    XDEBUG(printf("XFE_EditorDrop:insertFile(%s) at (%d,%d)\n",
                  data,_dropEventX,_dropEventY));

    const char *dataType=XFE_DragBase::guessUrlMimeType(data);

	MWContext* context = _editorView->getContext();

    if (XP_STRCASECMP(dataType,"image/gif") == 0
		||
		XP_STRCASECMP(dataType,"image/jpeg") == 0) {
        XDEBUG(printf("    GIF/JPG Image\n"));
		EDT_ImageData* image;
		char buf[1024];

		if (data[0] == '/' && data[1] != '/') { // local file name, url-ise it
			XP_STRCPY(buf, "file://");
			XP_STRNCAT_SAFE(buf, data, sizeof(buf) - 7/*sizeof("file://")*/);
			data = buf;
		}

		int32 x = _dropEventX + CONTEXT_DATA(context)->document_x;
		int32 y = _dropEventY + CONTEXT_DATA(context)->document_y;

		EDT_PositionCaret(context, x, y);

		image = EDT_NewImageData();
		image->pSrc = XP_STRDUP((char*)data); //need heap for EDT_FreeImageData
		image->bNoSave = FALSE;  // copy to be with file.

		EDT_InsertImage(context, image, FALSE);

		EDT_FreeImageData(image);

        return TRUE;
    } else {
		fe_EditorEdit(context, (XFE_Frame*)_editorView->getToplevel(),
					  /*chromespec=*/NULL, (char*)data);
		return TRUE;
	}
}

int XFE_EditorDrop::insertText(const char *data)
{
    XDEBUG(printf("XFE_EditorDrop:insertText(%s) at (%d,%d)\n",
                  data,_dropEventX,_dropEventY));
	MWContext* context = _editorView->getContext();

	int32 x = _dropEventX + CONTEXT_DATA(context)->document_x;
	int32 y = _dropEventY + CONTEXT_DATA(context)->document_y;

	EDT_PositionCaret(context, x, y);

	EDT_ClipboardResult result = EDT_PasteText(context, (char*)data);

	if (result != EDT_COP_OK) {
		XBell(XtDisplay(CONTEXT_WIDGET(context)), 0);
		return FALSE;
	}

    return TRUE;
}
