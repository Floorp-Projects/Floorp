/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * www.w3.org/TR/2012/WD-XMLHttpRequest-20120117/
 *
 * Copyright © 2012 W3C® (MIT, ERCIM, Keio), All Rights Reserved. W3C
 * liability, trademark and document use rules apply.
 */

[NoInterfaceObject]
interface XMLHttpRequestEventTarget : EventTarget {
  // event handlers
  [TreatNonCallableAsNull, SetterThrows, GetterThrows=Workers]
  attribute Function? onloadstart;

  [TreatNonCallableAsNull, SetterThrows, GetterThrows=Workers]
  attribute Function? onprogress;

  [TreatNonCallableAsNull, SetterThrows, GetterThrows=Workers]
  attribute Function? onabort;

  [TreatNonCallableAsNull, SetterThrows, GetterThrows=Workers]
  attribute Function? onerror;

  [TreatNonCallableAsNull, SetterThrows, GetterThrows=Workers]
  attribute Function? onload;

  [TreatNonCallableAsNull, SetterThrows, GetterThrows=Workers]
  attribute Function? ontimeout;

  [TreatNonCallableAsNull, SetterThrows, GetterThrows=Workers]
  attribute Function? onloadend;
};
