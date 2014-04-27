/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: Mozilla-sample-code 1.0
 *
 * Copyright (c) 2002 Netscape Communications Corporation and
 * other contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this Mozilla sample software and associated documentation files
 * (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the
 * following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Contributor(s):
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsIWebBrowserChrome.h"
#include "WindowCreator.h"
#include "winEmbed.h"

WindowCreator::WindowCreator()
{
}

WindowCreator::~WindowCreator()
{
}

NS_IMPL_ISUPPORTS(WindowCreator, nsIWindowCreator)

NS_IMETHODIMP
WindowCreator::CreateChromeWindow(nsIWebBrowserChrome *parent,
                                  uint32_t chromeFlags,
                                  nsIWebBrowserChrome **_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);
    AppCallbacks::CreateBrowserWindow(int32_t(chromeFlags), parent, _retval);
    return *_retval ? NS_OK : NS_ERROR_FAILURE;
}
