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

void CnsICmdParams::GetValueTypeTest(const char *aCommand, const char *stateType)
{
	PRInt16 retval;

	cmdParamObj = GetCommandParamObject();
	if (cmdParamObj) {
		FormatAndPrintOutput("The input command name = ", aCommand, 2);
		FormatAndPrintOutput("The input state type = ", stateType, 2);
		cmdMgrObj = CnsICommandMgr::GetCommandMgrObject(qaWebBrowser);
		if (!cmdMgrObj) {
			QAOutput("We didn't get nsICommandMgr object. Test fails.", 2);
			return;
		}
		else {
			rv = cmdMgrObj->GetCommandState(aCommand,cmdParamObj);
			RvTestResult(rv, "cmdMgrObj->GetCommandState test", 2);
		}
		rv = cmdParamObj->GetValueType(stateType, &retval);
		RvTestResult(rv, "GetValueType test", 2);
		FormatAndPrintOutput("GetValueType return int = ", retval, 2);
	}	
	else
	    QAOutput("GetValueTypeTest(): We didn't get nsICommandParams object.", 1);
}

void CnsICmdParams::GetBooleanValueTest(const char *aCommand, const char *stateType)
{
	PRBool retval;

	cmdParamObj = GetCommandParamObject();
	if (cmdParamObj) {
		FormatAndPrintOutput("The input command name = ", aCommand, 2);
		FormatAndPrintOutput("The input state type = ", stateType, 2);
		cmdMgrObj = CnsICommandMgr::GetCommandMgrObject(qaWebBrowser);
		if (!cmdMgrObj) {
			QAOutput("We didn't get nsICommandMgr object. Test fails.", 2);
			return;
		}
		else
			cmdMgrObj->GetCommandState(aCommand, cmdParamObj);

		rv = cmdParamObj->GetBooleanValue(stateType, &retval);
		RvTestResult(rv, "GetBooleanValue test", 2);
		FormatAndPrintOutput("GetBooleanValue() return boolean = ", retval, 2);
	}
	else
	    QAOutput("GetBooleanValueTest(): We didn't get nsICommandParams object.", 1);
}

void CnsICmdParams::GetLongValueTest(PRInt32 value, const char *stateType)
{
	PRInt32	retval;

	cmdParamObj = GetCommandParamObject();
	if (cmdParamObj)
	{
		FormatAndPrintOutput("The input Set value = ", value, 2);
		FormatAndPrintOutput("The input state type = ", stateType, 2);
		cmdParamObj->SetLongValue(stateType, value);
		rv = cmdParamObj->GetLongValue(stateType, &retval);
		RvTestResult(rv, "GetLongValue test", 2);
		FormatAndPrintOutput("GetLongValue() return long = ", retval, 2);
	}
	else
	    QAOutput("GetLongValueTest: We didn't get nsICommandParams object.", 1);
}

void CnsICmdParams::GetDoubleValueTest(double value, const char *stateType)
{
	double	retval;

	cmdParamObj = GetCommandParamObject();
	if (cmdParamObj) {
		FormatAndPrintOutput("The input Set value = ", value, 2);
		FormatAndPrintOutput("The input state type = ", stateType, 2);
		cmdParamObj->SetDoubleValue(stateType, value);
		rv = cmdParamObj->GetDoubleValue(stateType, &retval);
		RvTestResult(rv, "GetDoubleValue test", 2);
		FormatAndPrintOutput("GetLongValue() return double = ", retval, 2);
	}
	else
	    QAOutput("GetDoubleValueTest: We didn't get nsICommandParams object.", 1);
}

void CnsICmdParams::GetStringValueTest(char *stringVal, const char *stateType)
{
	nsAutoString retval;
	cmdParamObj = GetCommandParamObject();
	if (cmdParamObj) {
		FormatAndPrintOutput("The input string = ", stringVal, 2);
		FormatAndPrintOutput("The input state type = ", stateType, 2);

		rv = cmdParamObj->SetStringValue(stateType, NS_ConvertASCIItoUCS2(stringVal));		
		rv = cmdParamObj->GetStringValue(stateType, retval);
		RvTestResult(rv, "GetStringValue test", 2);
//		FormatAndPrintOutput("GetStringValue() return string = ", retval, 2);
	}
	else
	    QAOutput("GetStringValueTest: We didn't get nsICommandParams object.", 1);
}

void CnsICmdParams::GetCStringValueTest(const char *aCommand, const char *stateType)
{
	cmdParamObj = GetCommandParamObject();
	if (cmdParamObj) {
		FormatAndPrintOutput("The input command name = ", aCommand, 2);
		FormatAndPrintOutput("The input state type = ", stateType, 2);
		cmdMgrObj = CnsICommandMgr::GetCommandMgrObject(qaWebBrowser);
		if (!cmdMgrObj) {
			QAOutput("We didn't get nsICommandMgr object. Test fails.", 2);
			return;
		}
		else
			cmdMgrObj->GetCommandState(aCommand, cmdParamObj);
		char *tCstringValue = nsnull;
		rv = cmdParamObj->GetCStringValue(stateType, &tCstringValue);
		RvTestResult(rv, "GetCStringValue test", 2);
		FormatAndPrintOutput("the CString value = ", tCstringValue, 2);
		if (tCstringValue)
			nsCRT::free(tCstringValue);		
	}
	else
	    QAOutput("GetCStringValueTest: We didn't get nsICommandParams object.", 1);
}

// NS_ConvertASCIItoUCS2

void CnsICmdParams::SetBooleanValueTest(PRBool value, const char *stateType)
{
	cmdParamObj = GetCommandParamObject();
	if (cmdParamObj) {
		FormatAndPrintOutput("The input state type = ", stateType, 2);
		FormatAndPrintOutput("The input value = ", value, 2);
		rv = cmdParamObj->SetBooleanValue(stateType, value);
		RvTestResult(rv, "SetBooleanValue() test", 2);
	}
	else
	    QAOutput("SetBooleanValueTest(): We didn't get nsICommandParams object.", 1);
}

void CnsICmdParams::SetLongValueTest(PRInt32 value, const char *stateType)
{
	cmdParamObj = GetCommandParamObject();
	if (cmdParamObj) {
		FormatAndPrintOutput("The input state type = ", stateType, 2);
		FormatAndPrintOutput("The input value = ", value, 2);
		rv = cmdParamObj->SetLongValue(stateType, value);
		RvTestResult(rv, "SetLongValue() test", 2);
	}
	else
	    QAOutput("SetLongValueTest(): We didn't get nsICommandParams object.", 1);
}

void CnsICmdParams::SetDoubleValueTest(double value, const char *stateType)
{
	cmdParamObj = GetCommandParamObject();
	if (cmdParamObj) {
		FormatAndPrintOutput("The input state type = ", stateType, 2);
		FormatAndPrintOutput("The input value = ", value, 2);
		rv = cmdParamObj->SetDoubleValue(stateType, value);
		RvTestResult(rv, "SetDoubleValue() test", 2);
	}
	else
	    QAOutput("SetDoubleValueTest(): We didn't get nsICommandParams object.", 1);
}

void CnsICmdParams::SetStringValueTest(char *value, const char *stateType)
{
	cmdParamObj = GetCommandParamObject();
	if (cmdParamObj) {
		FormatAndPrintOutput("The input state type = ", stateType, 2);
		FormatAndPrintOutput("The input value = ", value, 2);
		rv = cmdParamObj->SetStringValue(stateType, NS_ConvertASCIItoUCS2(value));
		RvTestResult(rv, "SetStringValue() test", 2);
	}
	else
	    QAOutput("SetStringValueTest(): We didn't get nsICommandParams object.", 1);
}

void CnsICmdParams::SetCStringValueTest(char *value, const char *stateType)
{
	cmdParamObj = GetCommandParamObject();
	if (cmdParamObj) {
		FormatAndPrintOutput("The input state type = ", stateType, 2);
		FormatAndPrintOutput("The input value = ", value, 2);
		rv = cmdParamObj->SetCStringValue(stateType, value);
		RvTestResult(rv, "SetCStringValue() test", 2);
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
			GetValueTypeTest( "cmd_bold", "state_all");
			break;
		case ID_INTERFACES_NSICOMMANDPARAMS_GETBOOLEANVALUE :
			GetBooleanValueTest("cmd_bold", "state_mixed");
			break;
		case ID_INTERFACES_NSICOMMANDPARAMS_GETLONGVALUE  :
			GetLongValueTest(57000000, "state_attribute");
			break;
		case ID_INTERFACES_NSICOMMANDPARAMS_GETDOUBLEVALUE :
			GetDoubleValueTest(5.25, "state_attribute");
			break;
		case ID_INTERFACES_NSICOMMANDPARAMS_GETSTRINGVALUE :
			GetStringValueTest("MyString!!", "state_attribute");
			break;
		case ID_INTERFACES_NSICOMMANDPARAMS_GETCSTRINGVALUE:
			GetCStringValueTest("cmd_fontColor", "state_attribute");
			break;
		case ID_INTERFACES_NSICOMMANDPARAMS_GETISUPPORTSVALUE :
			QAOutput("Not implemented yet.", 2);
			break;
		case ID_INTERFACES_NSICOMMANDPARAMS_SETBOOLEANVALUE :
			SetBooleanValueTest(PR_TRUE, "state_attribute");
			break;
		case ID_INTERFACES_NSICOMMANDPARAMS_SETLONGVALUE :
			SetLongValueTest(25000000, "state_attribute");
			break;
		case ID_INTERFACES_NSICOMMANDPARAMS_SETDOUBLEVALUE :
			SetDoubleValueTest(-3.05, "state_attribute");
			break;
		case ID_INTERFACES_NSICOMMANDPARAMS_SETSTRINGVALUE :
			SetStringValueTest("HELLO!", "state_attribute");
			break;
		case ID_INTERFACES_NSICOMMANDPARAMS_SETCSTRINGVALUE:
			SetCStringValueTest("#FF0000", "state_attribute");
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
        QAOutput("We got nsICommandParams object.", 2);
	else
	    QAOutput("We didn't get nsICommandParams object.", 2);

	SetBooleanValueTest(PR_TRUE, "state_attribute");
	SetBooleanValueTest(PR_FALSE, "state_attribute");
	SetLongValueTest(15000000, "state_attribute");
	SetDoubleValueTest(100.295375, "state_attribute");
	SetStringValueTest("Hello world!", "state_attribute");
	QAOutput("Other tests Not implemented yet.", 2);
}


/////////////////////////////////////////////////////////////////////////////
// nsICmdParams message handlers
