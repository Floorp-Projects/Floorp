/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#include "nsHTMLParts.h"
#include "nsString.h"
#include "nsIAtom.h"
#include "nsHTMLTags.h"

nsresult
NS_CreateHTMLElement(nsIHTMLContent** aInstancePtrResult,
                     const nsString& aTag)
{
  // Find tag in tag table
  nsAutoString tmp(aTag);
  tmp.ToUpperCase();
  char cbuf[20];
  tmp.ToCString(cbuf, sizeof(cbuf));
  nsHTMLTag id = NS_TagToEnum(cbuf);
  if (eHTMLTag_userdefined == id) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // Create atom for tag and then create content object
  nsresult rv = NS_ERROR_NOT_AVAILABLE;
  nsIAtom* atom = NS_NewAtom(tmp);
  switch (id) {
  case eHTMLTag_a:
  case eHTMLTag_abbr:
  case eHTMLTag_acronym:
  case eHTMLTag_address:
  case eHTMLTag_b:
  case eHTMLTag_bdo:
  case eHTMLTag_big:
  case eHTMLTag_blink:
  case eHTMLTag_blockquote:
  case eHTMLTag_center:
  case eHTMLTag_cite:
  case eHTMLTag_code:
  case eHTMLTag_dd:
  case eHTMLTag_del:
  case eHTMLTag_dfn:
  case eHTMLTag_dir:
  case eHTMLTag_div:
  case eHTMLTag_dl:
  case eHTMLTag_dt:
  case eHTMLTag_em:
  case eHTMLTag_font:
  case eHTMLTag_h1:
  case eHTMLTag_h2:
  case eHTMLTag_h3:
  case eHTMLTag_h4:
  case eHTMLTag_h5:
  case eHTMLTag_h6:
  case eHTMLTag_i:
  case eHTMLTag_ilayer:
  case eHTMLTag_ins:
  case eHTMLTag_kbd:
  case eHTMLTag_layer:
  case eHTMLTag_li:
  case eHTMLTag_listing:
  case eHTMLTag_menu:
  case eHTMLTag_multicol:
  case eHTMLTag_nobr:
  case eHTMLTag_noembed:
  case eHTMLTag_noframes:
  case eHTMLTag_nolayer:
  case eHTMLTag_noscript:
  case eHTMLTag_ol:
  case eHTMLTag_p:
  case eHTMLTag_plaintext:
  case eHTMLTag_pre:
  case eHTMLTag_q:
  case eHTMLTag_s:
  case eHTMLTag_samp:
  case eHTMLTag_small:
  case eHTMLTag_span:
  case eHTMLTag_strike:
  case eHTMLTag_strong:
  case eHTMLTag_sub:
  case eHTMLTag_sup:
  case eHTMLTag_tt:
  case eHTMLTag_u:
  case eHTMLTag_ul:
  case eHTMLTag_var:
  case eHTMLTag_xmp:
    rv = NS_NewHTMLContainer(aInstancePtrResult, atom);
    break;

  case eHTMLTag_body:
    rv = NS_NewHTMLContainer(aInstancePtrResult, atom);
    break;
  case eHTMLTag_br:
    rv = NS_NewHTMLBreak(aInstancePtrResult, atom);
    break;
  case eHTMLTag_colgroup:
    rv = NS_NewTableColGroupPart(aInstancePtrResult, atom);
    break;
  case eHTMLTag_head:
    rv = NS_NewHTMLHead(aInstancePtrResult, atom);
    break;
  case eHTMLTag_hr:
    rv = NS_NewHRulePart(aInstancePtrResult, atom);
    break;
  case eHTMLTag_html:
    break;
  case eHTMLTag_img:
    rv = NS_NewHTMLImage(aInstancePtrResult, atom);
    break;
  case eHTMLTag_meta:
    rv = NS_NewHTMLMeta(aInstancePtrResult, atom);
    break;
  case eHTMLTag_object:
    break;
  case eHTMLTag_spacer:
    rv = NS_NewHTMLSpacer(aInstancePtrResult, atom);
    break;
  case eHTMLTag_table:
    rv = NS_NewTablePart(aInstancePtrResult, atom);
    break;
  case eHTMLTag_tbody:
  case eHTMLTag_tfoot:
  case eHTMLTag_thead:
    rv = NS_NewTableRowGroupPart(aInstancePtrResult, atom);
    break;
  case eHTMLTag_td:
  case eHTMLTag_th:
    rv = NS_NewTableCellPart(aInstancePtrResult, atom);
    break;
  case eHTMLTag_tr:
    rv = NS_NewTableRowPart(aInstancePtrResult, atom);
    break;
  case eHTMLTag_wbr:
    rv = NS_NewHTMLWordBreak(aInstancePtrResult, atom);
    break;
  }

  NS_RELEASE(atom);
  return rv;
}

