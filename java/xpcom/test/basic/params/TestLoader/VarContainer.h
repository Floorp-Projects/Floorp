#include<stdio.h>
#include <stdlib.h>
#include <stack>
#include "prtypes.h"
#include "Values.h"

typedef std::stack<PRInt16> PRInt16Stack;
typedef std::stack<PRInt32> PRInt32Stack;
typedef std::stack<PRInt64> PRInt64Stack;
typedef std::stack<PRUint8> PRUint8Stack;
typedef std::stack<PRUint16> PRUint16Stack;
typedef std::stack<PRUint32> PRUint32Stack;
typedef std::stack<PRUint64> PRUint64Stack;
typedef std::stack<float> floatStack;
typedef std::stack<double> doubleStack;
typedef std::stack<PRBool> PRBoolStack;
typedef std::stack<char> charStack;
typedef std::stack<char> wcharStack;
typedef std::stack<char*> stringStack;

#define DECL_VAR_STACKS \
NS_IMETHOD InitStackVars(); \
PRInt16Stack  PRInt16Vars; \
PRInt32Stack  PRInt32Vars; \
PRInt64Stack  PRInt64Vars; \
PRUint8Stack  PRUint8Vars; \
PRUint16Stack PRUint16Vars; \
PRUint32Stack PRUint32Vars; \
PRUint64Stack PRUint64Vars; \
floatStack    floatVars; \
doubleStack   doubleVars; \
PRBoolStack   PRBoolVars; \
charStack   charVars; \
wcharStack   wcharVars; \
stringStack stringVars; \

#define IMPL_VAR_STACKS(_MYCLASS_) \
 \
NS_IMETHODIMP _MYCLASS_::InitStackVars() { \
PRInt16Vars.push(short_min); \
PRInt16Vars.push(short_mid); \
PRInt16Vars.push(short_max); \
PRInt16Vars.push(short_zerro); \
PRInt32Vars.push(int_min); \
PRInt32Vars.push(int_mid); \
PRInt32Vars.push(int_max); \
PRInt32Vars.push(int_zerro); \
PRInt64Vars.push(long_min); \
PRInt64Vars.push(long_mid); \
PRInt64Vars.push(long_max); \
PRInt64Vars.push(long_zerro); \
PRUint8Vars.push(byte_min); \
PRUint8Vars.push(byte_mid); \
PRUint8Vars.push(byte_max); \
PRUint16Vars.push(ushort_min); \
PRUint16Vars.push(ushort_mid); \
PRUint16Vars.push(ushort_max); \
PRUint32Vars.push(uint_min); \
PRUint32Vars.push(uint_mid); \
PRUint32Vars.push(uint_max); \
PRUint64Vars.push(ulong_min); \
PRUint64Vars.push(ulong_mid); \
PRUint64Vars.push(ulong_max); \
floatVars.push(float_min); \
floatVars.push(float_mid); \
floatVars.push(float_max); \
floatVars.push(float_zerro); \
doubleVars.push(double_min); \
doubleVars.push(double_mid); \
doubleVars.push(double_max); \
doubleVars.push(double_zerro); \
PRBoolVars.push(PR_TRUE); \
PRBoolVars.push(PR_FALSE); \
charVars.push(char_first); \
charVars.push(char_last); \
wcharVars.push(wchar_first); \
wcharVars.push(wchar_last); \
stringVars.push(string_last); \
stringVars.push(string_first); \
stringVars.push(string_empty); \
stringVars.push(string_null); \
return NS_OK; \
}
