/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

callback TestUnionArguments = undefined((DOMString or long) arg1,
                                        (DOMString or long)? arg2,
                                        optional (DOMString or long) arg3,
                                        optional (DOMString or long)? arg4);
