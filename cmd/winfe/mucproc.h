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

#ifndef MUCPROC_H
#define MUCPROC_H

 typedef long (STDAPICALLTYPE *FARPEFUNC)(long, void*, void*); 

enum
{
	kGetPluginVersion,
	kSelectAcctConfig,
	kSelectModemConfig,
	kSelectDialOnDemand
};

class CMucProc
{
public:

	CMucProc();

	BOOL GetAcctArray(CStringArray *acctList);
	BOOL IsAcctValid(char* acctStr);

	BOOL GetModemArray(CStringArray *modemList);
	BOOL IsModemValid(char* modemStr);

	BOOL LoadMuc();

	void SetDialOnDemand(CString, BOOL);

	FARPEFUNC m_lpfnPEPluginFunc;

protected:
	BOOL GetModemNames();
	BOOL GetAccountNames();

private:
	CStringArray    acctNames;              // array of accounts
	CStringArray    modemNames;             // array of modems
	BOOL                    acct_flag;              // flag of acctNames availability
	BOOL                    modem_flag;             // flags of modemNames availability
};

#endif
