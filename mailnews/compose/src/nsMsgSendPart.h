/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef _MsgSendPart_H_
#define _MsgSendPart_H_

#include "msgCore.h"
#include "prprf.h" /* should be defined into msgCore.h? */
#include "xp_core.h"
#include "nsMsgSend.h"

typedef int (*MSG_SendPartWriteFunc)(const char* line, PRInt32 size,
									                   PRBool isheader, void* closure);

class nsMsgSendPart {
public:
    nsMsgSendPart(nsIMsgSend* state, const char *part_charset = NULL);
    virtual ~nsMsgSendPart();	  // Note that the destructor also destroys
								                // any children that were added.

    virtual int       Write();

    virtual int       SetFile(nsFileSpec *filename);
    const nsFileSpec  *GetFileSpec() {return m_filespec;}

    virtual int       SetBuffer(const char* buffer);
    const char        *GetBuffer() {return m_buffer;}

    virtual int       SetType(const char* type);
    const char        *GetType() {return m_type;}
    
    const char        *GetCharsetName() {return m_charset_name;}

    virtual int       SetOtherHeaders(const char* other);
    const char        *SetOtherHeaders() {return m_other;}
	  virtual int       AppendOtherHeaders(const char* moreother);

	  virtual int       SetMimeDeliveryState(nsIMsgSend* state);

	// Note that the nsMsgSendPart class will take over ownership of the
	// MimeEncoderData* object, deleting it when it chooses.  (This is
	// necessary because deleting these objects is the only current way to
	// flush out the data in them.)
	int                 SetEncoderData(MimeEncoderData* data);
	MimeEncoderData     *GetEncoderData() {return m_encoder_data;}

	int                 SetStripSensitiveHeaders(PRBool value) 
                      {
		                    m_strip_sensitive_headers = value;
		                    return 0;
	                    }
	PRBool              GetStripSensitiveHeaders() {return m_strip_sensitive_headers;}

  virtual int         AddChild(nsMsgSendPart* child);

	PRInt32             GetNumChildren() {return m_numchildren;}
	nsMsgSendPart       *GetChild(PRInt32 which);
	nsMsgSendPart       *DetachChild(PRInt32 which);

	virtual int         SetMainPart(PRBool value);
	PRBool              IsMainPart() 
                      {
                        return m_mainpart;
                      }
protected:
	int                 CopyString(char** dest, const char* src);
	int                 PushBody(char* buffer, PRInt32 length);

	nsCOMPtr<nsIMsgSend> m_state;
	nsMsgSendPart       *m_parent;
  nsFileSpec          *m_filespec;
	char                *m_buffer;
  char                *m_type;
  char                *m_other;
  char                m_charset_name[64+1];        // charset name associated with this part
	PRBool              m_strip_sensitive_headers;
	MimeEncoderData     *m_encoder_data;  /* Opaque state for base64/qp encoder. */

	nsMsgSendPart       **m_children;
	PRInt32             m_numchildren;

	// Data used while actually writing.
  PRBool              m_firstBlock;
  PRBool              m_needIntlConversion;

	PRBool              m_mainpart;

	PRBool              m_just_hit_CR;

	static PRInt32      M_counter;
};

#endif /* _MsgSendPart_H_ */
