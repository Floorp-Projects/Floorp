/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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

#ifndef _NS_USER_DIALOG_HELPER_H_
#define _NS_USER_DIALOG_HELPER_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "prtypes.h"

typedef enum nsRiskType {
  nsRiskType_LowRisk=10,
  nsRiskType_MediumRisk=20,
  nsRiskType_HighRisk=30
} nsRiskType;

extern char * JavaSecUI_targetRiskStr(int risk);
extern int    JavaSecUI_targetRiskLow(void);
extern int    JavaSecUI_targetRiskMedium(void);
extern int    JavaSecUI_targetRiskHigh(void);

extern char * JavaSecUI_getHelpURL(int id);
extern char * JavaSecUI_getString(int id);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* _NS_USER_DIALOG_HELPER_H_ */
