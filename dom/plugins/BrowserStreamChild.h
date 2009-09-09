/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 8 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Plugin App.
 *
 * The Initial Developer of the Original Code is
 *   Benjamin Smedberg <benjamin@smedbergs.us>
 * Portions created by the Initial Developer are Copyright (C) 2009
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

#ifndef mozilla_plugins_BrowserStreamChild_h
#define mozilla_plugins_BrowserStreamChild_h 1

#include "mozilla/plugins/PBrowserStreamChild.h"

namespace mozilla {
namespace plugins {

class PluginInstanceChild;

class BrowserStreamChild : public PBrowserStreamChild
{
public:
  BrowserStreamChild(PluginInstanceChild* instance,
                     const nsCString& url,
                     const uint32_t& length,
                     const uint32_t& lastmodified,
                     const nsCString& headers,
                     const nsCString& mimeType,
                     const bool& seekable,
                     NPError* rv,
                     uint16_t* stype);
  virtual ~BrowserStreamChild() { }

  virtual nsresult AnswerNPP_WriteReady(const int32_t& newlength,
                                        int32_t *size);
  virtual nsresult AnswerNPP_Write(const int32_t& offset,
                                   const Buffer& data,
                                   int32_t* consumed);

  virtual nsresult AnswerNPP_StreamAsFile(const nsCString& fname);

private:
  PluginInstanceChild* mInstance;
  NPStream mStream;
  bool mClosed;
  nsCString mURL;
  nsCString mHeaders;
};

} // namespace plugins
} // namespace mozilla

#endif /* mozilla_plugins_BrowserStreamChild_h */
