// nsICmdParams.cpp : implementation file
//

#include "stdafx.h"
#include "testembed.h"
#include "nsICmdParams.h"
#include "QaUtils.h"
#include "BrowserFrm.h"
#include "BrowserImpl.h"
#include "BrowserView.h"
#include "Tests.h"
#include "nsICommandMgr.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// nsICmdParams

// note: this table is temporary. will customize later.
// 1st column: command; 2nd column: DoCommand state, 3rd column: CmdParam state; 


CnsICmdParams::CnsICmdParams(nsIWebBrowser *mWebBrowser)
{
	qaWebBrowser = mWebBrowser;
}

CnsICmdParams::~CnsICmdParams()
{
}

nsICommandParams * CnsICmdParams::GetCommandParamObject()
{
    nsCOMPtr<nsICommandParams> cmdParamsObj = do_CreateInstance(NS_COMMAND_PARAMS_CONTRACTID,&rv);
	RvTestResult(rv, "GetCommandParamsObject() test", 1); 
    if (!cmdParamsObj) {
        QAOutput("Didn't get nsICommandParams object.");
		return nsnull;
	}
	nsICommandParams *retVal = cmdParamsObj;
	NS_ADDREF(retVal);
	return retVal;
}

void CnsICmdParams::GetValueTypeTest(const char *aCommand, const char *stateType, int displayMode)
{
	PRInt16 retval;

	cmdParamObj = GetCommandParamObject();
	if (cmdParamObj) {
		FormatAndPrintOutput("The input command name = ", aCommand, displayMode);
		FormatAndPrintOutput("The input state type = ", stateType, displayMode);
		cmdMgrObj = CnsICommandMgr::GetCommandMgrObject(qaWebBrowser);
		if (!cmdMgrObj) {
			QAOutput("We didn't get nsICommandMgr object. Test fails.", displayMode);
			return;
		}
		else {
			rv = cmdMgrObj->GetCommandState(aCommand, nsnull, cmdParamObj);
			RvTestResult(rv, "cmdMgrObj->GetCommandState test", displayMode);
		}
		rv = cmdParamObj->GetValueType(stateType, &retval);
		RvTestResult(rv, "GetValueType test", displayMode);
		FormatAndPrintOutput("GetValueType return int = ", retval, displayMode);
	}	
	else
	    QAOutput("GetValueTypeTest(): We didn't get nsICommandParams object.", 1);
}

void CnsICmdParams::GetBooleanValueTest(const char *aCommand, const char *stateType, int displayMode)
{
	PRBool retval;

	cmdParamObj = GetCommandParamObject();
	if (cmdParamObj) {
		FormatAndPrintOutput("The input command name = ", aCommand, displayMode);
		FormatAndPrintOutput("The input state type = ", stateType, displayMode);
		cmdMgrObj = CnsICommandMgr::GetCommandMgrObject(qaWebBrowser);
		if (!cmdMgrObj) {
			QAOutput("We didn't get nsICommandMgr object. Test fails.", displayMode);
			return;
		}
		else
			cmdMgrObj->GetCommandState(aCommand, nsnull, cmdParamObj);

		rv = cmdParamObj->GetBooleanValue(stateType, &retval);
		RvTestResult(rv, "GetBooleanValue test", displayMode);
		FormatAndPrintOutput("GetBooleanValue() return boolean = ", retval, displayMode);
	}
	else
	    QAOutput("GetBooleanValueTest(): We didn't get nsICommandParams object.", 1);
}

void CnsICmdParams::GetLongValueTest(PRInt32 value, const char *stateType, int displayMode)
{
	PRInt32	retval;

	cmdParamObj = GetCommandParamObject();
	if (cmdParamObj)
	{
		FormatAndPrintOutput("The input Set value = ", value, displayMode);
		FormatAndPrintOutput("The input state type = ", stateType, displayMode);
		cmdParamObj->SetLongValue(stateType, value);
		rv = cmdParamObj->GetLongValue(stateType, &retval);
		RvTestResult(rv, "GetLongValue test", displayMode);
		FormatAndPrintOutput("GetLongValue() return long = ", retval, displayMode);
	}
	else
	    QAOutput("GetLongValueTest: We didn't get nsICommandParams object.", 1);
}

void CnsICmdParams::GetDoubleValueTest(double value, const char *stateType, int displayMode)
{
	double	retval;

	cmdParamObj = GetCommandParamObject();
	if (cmdParamObj) {
		FormatAndPrintOutput("The input Set value = ", value, displayMode);
		FormatAndPrintOutput("The input state type = ", stateType, displayMode);
		cmdParamObj->SetDoubleValue(stateType, value);
		rv = cmdParamObj->GetDoubleValue(stateType, &retval);
		RvTestResult(rv, "GetDoubleValue test", displayMode);
		FormatAndPrintOutput("GetLongValue() return double = ", retval, displayMode);
	}
	else
	    QAOutput("GetDoubleValueTest: We didn't get nsICommandParams object.", 1);
}

void CnsICmdParams::GetStringValueTest(char *stringVal, const char *stateType, int displayMode)
{
	nsAutoString retval;
	cmdParamObj = GetCommandParamObject();
	if (cmdParamObj) {
		FormatAndPrintOutput("The input string = ", stringVal, displayMode);
		FormatAndPrintOutput("The input state type = ", stateType, displayMode);

		rv = cmdParamObj->SetStringValue(stateType, NS_ConvertASCIItoUCS2(stringVal));		
		rv = cmdParamObj->GetStringValue(stateType, retval);
		RvTestResult(rv, "GetStringValue test", displayMode);
//		FormatAndPrintOutput("GetStringValue() return string = ", retval, displayMode);
	}
	else
	    QAOutput("GetStringValueTest: We didn't get nsICommandParams object.", 1);
}

void CnsICmdParams::GetCStringValueTest(const char *aCommand, const char *stateType, int displayMode)
{
	cmdParamObj = GetCommandParamObject();
	if (cmdParamObj) {
		FormatAndPrintOutput("The input command name = ", aCommand, displayMode);
		FormatAndPrintOutput("The input state type = ", stateType, displayMode);
		cmdMgrObj = CnsICommandMgr::GetCommandMgrObject(qaWebBrowser);
		if (!cmdMgrObj) {
			QAOutput("We didn't get nsICommandMgr object. Test fails.", displayMode);
			return;
		}
		else
			cmdMgrObj->GetCommandState(aCommand, nsnull, cmdParamObj);
		char *tCstringValue = nsnull;
		rv = cmdParamObj->GetCStringValue(stateType, &tCstringValue);
		RvTestResult(rv, "GetCStringValue test", displayMode);
		FormatAndPrintOutput("the CString value = ", tCstringValue, displayMode);
		if (tCstringValue)
			nsCRT::free(tCstringValue);		
	}
	else
	    QAOutput("GetCStringValueTest: We didn't get nsICommandParams object.", 1);
}

// NS_ConvertASCIItoUCS2

void CnsICmdParams::SetBooleanValueTest(PRBool value, const char *stateType, int displayMode)
{
	cmdParamObj = GetCommandParamObject();
	if (cmdParamObj) {
		FormatAndPrintOutput("The input state type = ", stateType, displayMode);
		FormatAndPrintOutput("The input value = ", value, displayMode);
		rv = cmdParamObj->SetBooleanValue(stateType, value);
		RvTestResult(rv, "SetBooleanValue() test", displayMode);
	}
	else
	    QAOutput("SetBooleanValueTest(): We didn't get nsICommandParams object.", 1);
}

void CnsICmdParams::SetLongValueTest(PRInt32 value, const char *stateType, int displayMode)
{
	cmdParamObj = GetCommandParamObject();
	if (cmdParamObj) {
		FormatAndPrintOutput("The input state type = ", stateType, displayMode);
		FormatAndPrintOutput("The input value = ", value, displayMode);
		rv = cmdParamObj->SetLongValue(stateType, value);
		RvTestResult(rv, "SetLongValue() test", displayMode);
	}
	else
	    QAOutput("SetLongValueTest(): We didn't get nsICommandParams object.", 1);
}

void CnsICmdParams::SetDoubleValueTest(double value, const char *stateType, int displayMode)
{
	cmdParamObj = GetCommandParamObject();
	if (cmdParamObj) {
		FormatAndPrintOutput("The input state type = ", stateType, displayMode);
		FormatAndPrintOutput("The input value = ", value, displayMode);
		rv = cmdParamObj->SetDoubleValue(stateType, value);
		RvTestResult(rv, "SetDoubleValue() test", displayMode);
	}
	else
	    QAOutput("SetDoubleValueTest(): We didn't get nsICommandParams object.", 1);
}

void CnsICmdParams::SetStringValueTest(char *value, const char *stateType, int displayMode)
{
	cmdParamObj = GetCommandParamObject();
	if (cmdParamObj) {
		FormatAndPrintOutput("The input state type = ", stateType, displayMode);
		FormatAndPrintOutput("The input value = ", value, displayMode);
		rv = cmdParamObj->SetStringValue(stateType, NS_ConvertASCIItoUCS2(value));
		RvTestResult(rv, "SetStringValue() test", displayMode);
	}
	else
	    QAOutput("SetStringValueTest(): We didn't get nsICommandParams object.", 1);
}

void CnsICmdParams::SetCStringValueTest(char *value, const char *stateType, int displayMode)
{
	cmdParamObj = GetCommandParamObject();
	if (cmdParamObj) {
		FormatAndPrintOutput("The input state type = ", stateType, displayMode);
		FormatAndPrintOutput("The input value = ", value, displayMode);
		rv = cmdParamObj->SetCStringValue(stateType, value);
		RvTestResult(rv, "SetCStringValue() test", displayMode);
	}
	else
	    QAOutput("SetCStringValueTest(): We didn't get nsICommandParams object.", 1);
}

void CnsICmdParams::OnStartTests(UINT nMenuID)
{
	switch(nMenuID)
	{
		case ID_INTERFACES_NSICOMMANDPARAMS_RUNALLTESTS :
			RunAllTests();
			break;
		case ID_INTERFACES_NSICOMMANDPARAMS_GETVALUETYPE :
			GetValueTypeTest( "cmd_bold", "state_all", 2);
			break;
		case ID_INTERFACES_NSICOMMANDPARAMS_GETBOOLEANVALUE :
			GetBooleanValueTest("cmd_bold", "state_mixed", 2);
			break;
		case ID_INTERFACES_NSICOMMANDPARAMS_GETLONGVALUE  :
			GetLongValueTest(57000000, "state_attribute", 2);
			break;
		case ID_INTERFACES_NSICOMMANDPARAMS_GETDOUBLEVALUE :
			GetDoubleValueTest(5.25, "state_attribute", 2);
			break;
		case ID_INTERFACES_NSICOMMANDPARAMS_GETSTRINGVALUE :
			GetStringValueTest("MyString!!", "state_attribute", 2);
			break;
		case ID_INTERFACES_NSICOMMANDPARAMS_GETCSTRINGVALUE:
			GetCStringValueTest("cmd_fontColor", "state_attribute", 2);
			break;
		case ID_INTERFACES_NSICOMMANDPARAMS_GETISUPPORTSVALUE :
			QAOutput("Not implemented yet.", 2);
			break;
		case ID_INTERFACES_NSICOMMANDPARAMS_SETBOOLEANVALUE :
			SetBooleanValueTest(PR_TRUE, "state_attribute", 2);
			break;
		case ID_INTERFACES_NSICOMMANDPARAMS_SETLONGVALUE :
			SetLongValueTest(25000000, "state_attribute", 2);
			break;
		case ID_INTERFACES_NSICOMMANDPARAMS_SETDOUBLEVALUE :
			SetDoubleValueTest(-3.05, "state_attribute", 2);
			break;
		case ID_INTERFACES_NSICOMMANDPARAMS_SETSTRINGVALUE :
			SetStringValueTest("HELLO!", "state_attribute", 2);
			break;
		case ID_INTERFACES_NSICOMMANDPARAMS_SETCSTRINGVALUE:
			SetCStringValueTest("#FF0000", "state_attribute", 2);
			break;
		case ID_INTERFACES_NSICOMMANDPARAMS_SETISUPPORTSVALUE :
			QAOutput("Not implemented yet.", 2);
			break;
		case ID_INTERFACES_NSICOMMANDPARAMS_REMOVEVALUE :
			QAOutput("Not implemented yet.", 2);
			break;
		case ID_INTERFACES_NSICOMMANDPARAMS_HASMOREELEMENTS :
			QAOutput("Not implemented yet.", 2);
			break;
		case ID_INTERFACES_NSICOMMANDPARAMS_FIRST :
			QAOutput("Not implemented yet.", 2);
			break;
		case ID_INTERFACES_NSICOMMANDPARAMS_GETNEXT :
			QAOutput("Not implemented yet.", 2);
			break;
	}
}

void CnsICmdParams::RunAllTests()
{
	cmdParamObj = GetCommandParamObject();
	if (cmdParamObj)
        QAOutput("We got nsICommandParams object.", 1);
	else
	    QAOutput("We didn't get nsICommandParams object.", 1);

	PRInt16 i;

	QAOutput("Run All nsICommandParam Set and Get tests. Check C:/Temp/TestOutput.txt for test results.", 2);
	for (i=0; i < 50; i++)
	{
		FormatAndPrintOutput("loop cnt = ", i, 1);
		SetBooleanValueTest(CommandTable[i].mBooleanValue, CommandTable[i].mCmdParamState, 1);
		SetLongValueTest(CommandTable[i].mLongValue, CommandTable[i].mCmdParamState, 1);
		SetDoubleValueTest(CommandTable[i].mDoubleValue, CommandTable[i].mCmdParamState, 1);
		SetStringValueTest(CommandTable[i].mStringValue, CommandTable[i].mCmdParamState, 1);
		SetCStringValueTest(CommandTable[i].mCStringValue, CommandTable[i].mCmdParamState, 1);

		GetValueTypeTest(CommandTable[i].mCmdName, CommandTable[i].mCmdParamState, 1);
		GetBooleanValueTest(CommandTable[i].mCmdName, CommandTable[i].mCmdParamState, 1);
		GetLongValueTest(CommandTable[i].mLongValue, CommandTable[i].mCmdParamState, 1);
		GetDoubleValueTest(CommandTable[i].mDoubleValue, CommandTable[i].mCmdParamState, 1);
		GetStringValueTest(CommandTable[i].mStringValue, CommandTable[i].mCmdParamState, 1);
		GetCStringValueTest(CommandTable[i].mCmdName, CommandTable[i].mCmdParamState, 1);
	}
	QAOutput("Other tests Not implemented yet.", 2);
}

/////////////////////////////////////////////////////////////////////////////
// nsICmdParams message handlers
