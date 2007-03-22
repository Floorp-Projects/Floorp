
const TestVariant = Components.Constructor("@mozilla.org/js/xpc/test/TestVariant;1", 
                                           "nsITestVariant");

var tv = new TestVariant;

const DataTypeArray = [
    Components.interfaces.nsIDataType.VTYPE_INT8                ,
    Components.interfaces.nsIDataType.VTYPE_INT16               ,
    Components.interfaces.nsIDataType.VTYPE_INT32               ,
    Components.interfaces.nsIDataType.VTYPE_INT64               ,
    Components.interfaces.nsIDataType.VTYPE_UINT8               ,
    Components.interfaces.nsIDataType.VTYPE_UINT16              ,
    Components.interfaces.nsIDataType.VTYPE_UINT32              ,
    Components.interfaces.nsIDataType.VTYPE_UINT64              ,
    Components.interfaces.nsIDataType.VTYPE_FLOAT               ,
    Components.interfaces.nsIDataType.VTYPE_DOUBLE              ,
    Components.interfaces.nsIDataType.VTYPE_BOOL                ,
    Components.interfaces.nsIDataType.VTYPE_CHAR                ,
    Components.interfaces.nsIDataType.VTYPE_WCHAR               ,
    Components.interfaces.nsIDataType.VTYPE_VOID                ,
    Components.interfaces.nsIDataType.VTYPE_ID                  ,
    Components.interfaces.nsIDataType.VTYPE_DOMSTRING           ,
    Components.interfaces.nsIDataType.VTYPE_CHAR_STR            ,
    Components.interfaces.nsIDataType.VTYPE_WCHAR_STR           ,
    Components.interfaces.nsIDataType.VTYPE_INTERFACE           ,
    Components.interfaces.nsIDataType.VTYPE_INTERFACE_IS        ,
    Components.interfaces.nsIDataType.VTYPE_ARRAY               ,
    Components.interfaces.nsIDataType.VTYPE_STRING_SIZE_IS      ,
    Components.interfaces.nsIDataType.VTYPE_WSTRING_SIZE_IS     ,
    Components.interfaces.nsIDataType.VTYPE_UTF8STRING          ,
    Components.interfaces.nsIDataType.VTYPE_CSTRING             ,
    Components.interfaces.nsIDataType.VTYPE_ASTRING             ,    
    Components.interfaces.nsIDataType.VTYPE_EMPTY_ARRAY         ,               
    Components.interfaces.nsIDataType.VTYPE_EMPTY               
];

const ShortNames = [
 {name: "I1", number: Components.interfaces.nsIDataType.VTYPE_INT8           },
 {name: "I2", number: Components.interfaces.nsIDataType.VTYPE_INT16          },
 {name: "I4", number: Components.interfaces.nsIDataType.VTYPE_INT32          },
 {name: "I8", number: Components.interfaces.nsIDataType.VTYPE_INT64          },
 {name: "U1", number: Components.interfaces.nsIDataType.VTYPE_UINT8          },
 {name: "U2", number: Components.interfaces.nsIDataType.VTYPE_UINT16         },
 {name: "U4", number: Components.interfaces.nsIDataType.VTYPE_UINT32         },
 {name: "U8", number: Components.interfaces.nsIDataType.VTYPE_UINT64         },
 {name: "FL", number: Components.interfaces.nsIDataType.VTYPE_FLOAT          },
 {name: "DB", number: Components.interfaces.nsIDataType.VTYPE_DOUBLE         },
 {name: "BO", number: Components.interfaces.nsIDataType.VTYPE_BOOL           },
 {name: "CH", number: Components.interfaces.nsIDataType.VTYPE_CHAR           },
 {name: "WC", number: Components.interfaces.nsIDataType.VTYPE_WCHAR          },
 {name: "VD", number: Components.interfaces.nsIDataType.VTYPE_VOID           },
 {name: "ID", number: Components.interfaces.nsIDataType.VTYPE_ID             },
 {name: "DS", number: Components.interfaces.nsIDataType.VTYPE_DOMSTRING      },
 {name: "ST", number: Components.interfaces.nsIDataType.VTYPE_CHAR_STR       },
 {name: "WS", number: Components.interfaces.nsIDataType.VTYPE_WCHAR_STR      },
 {name: "NS", number: Components.interfaces.nsIDataType.VTYPE_INTERFACE      },
 {name: "IF", number: Components.interfaces.nsIDataType.VTYPE_INTERFACE_IS   },
 {name: "AR", number: Components.interfaces.nsIDataType.VTYPE_ARRAY          },
 {name: "Ss", number: Components.interfaces.nsIDataType.VTYPE_STRING_SIZE_IS },
 {name: "Ws", number: Components.interfaces.nsIDataType.VTYPE_WSTRING_SIZE_IS},
 {name: "US", number: Components.interfaces.nsIDataType.VTYPE_UTF8STRING     },
 {name: "CS", number: Components.interfaces.nsIDataType.VTYPE_CSTRING        },
 {name: "AS", number: Components.interfaces.nsIDataType.VTYPE_ASTRING        },
 {name: "EA", number: Components.interfaces.nsIDataType.VTYPE_EMPTY_ARRAY    },
 {name: "EM", number: Components.interfaces.nsIDataType.VTYPE_EMPTY          }
];


function getDataTypeName(number) {
    var iface = Components.interfaces.nsIDataType
    for(var n in iface)
        if(iface[n] == number)
            return n;
    return "unknown type!";
}

function getDataTypeShortName(number) {
    for(var i = 0; i < ShortNames.length; i++)
        if(ShortNames[i].number == number)
            return ShortNames[i].name;
    return "???";
}

function eqOp(o1, o2) {return o1 == o2;}
function eqObj(o1, o2) {return o1.equals(o2);}
function eqNumber(o1, o2) {return parseFloat(o1) == parseFloat(o2);}

const eq = {string: "equal result"};
const _e = {string: "exception"};
const NE = {string: "un-equal result"};
const _0 = {string: "zero result"};
const _1 = {string: "one result"};
const _T = {string: "true result"};
const _F = {string: "false result"};

function TestSingleConvert(value, comment, eq_fun, table) {
    print("<h3>convert test for: "+value+" "+comment+"</h3>");

    print('<TABLE BORDER="1" COLS='+table.length+'>');
    print('<TR>');
    for(var i = 0; i < table.length; i++) {
        print('<TH>'+getDataTypeShortName(DataTypeArray[i])+'</TH>');
    }
    print('</TR>');
    
    print('<TR>');
    for(var i = 0; i < table.length; i++) {
        var exception = undefined;
        var value2 = undefined;
        try {
            value2 = tv.copyVariantAsType(value, DataTypeArray[i]);
            var same = eq_fun(value, value2);
            success = (same && table[i] == eq) || 
                      (!same && table[i] == NE) ||
                      (table[i] == _0 && 0 == value2) ||
                      (table[i] == _T && true == value2) ||
                      (table[i] == _F && false == value2);
        } catch(e) {
            exception = e;
            success = table[i] == _e;
        }                
        if(success)   
            print('<TD><font color="green">OK</font></TD>');
        else if(exception) {
            var alertText = "Exception thrown. Expected: "+table[i].string;
            print('<TD><font color="red"><A HREF="" '+
                  'onclick="alert(\''+alertText+'\'); return false;"'+
                  '>X</A></font></TD>');
        }
        else {
            var alertText = "Result = "+value2+". Expected: "+table[i].string;
            print('<TD><font color="red"><A HREF="" '+
                  'onclick="alert(\''+alertText+'\'); return false;"'+
                  '>X</A></font></TD>');
        }            
    }    

    print('</TR>');
    print('</TABLE>');
}

function TestDoubleConvert(value, comment, eq_fun, table) {
    print("<h3>convert test for: "+value+" "+comment+"</h3>");

    print('<TABLE BORDER="1" COLS='+table.length+2+'>');
    print('<TR>');
    print('<TH></TH>');
    for(var i = 0; i < table.length; i++) {
        print('<TH>'+getDataTypeShortName(DataTypeArray[i])+'</TH>');
    }
    print('<TH></TH>');
    print('</TR>');
    
    for(var i = 0; i < table.length; i++) {
        print('<TR>');
        print('<TD>'+getDataTypeShortName(DataTypeArray[i])+'</TD>');
        for(var k = 0; k < table.length; k++) {
            var exception = undefined;
            var value2 = undefined;
            var expected = table[i][k];
            try {
                value2 = tv.copyVariantAsTypeTwice(value, 
                                                   DataTypeArray[k],
                                                   DataTypeArray[i]);
                var same = eq_fun(value, value2);
                success = (same && expected == eq) || 
                          (!same && expected == NE) ||
                          (expected == _0 && 0 == value2) ||
                          (expected == _T && true == value2) ||
                          (expected == _F && false == value2);
            } catch(e) {
                exception = e;
                success = expected == _e;
            }                
            if(success)   
                print('<TD><font color="green">OK</font></TD>');
            else if(exception) {
                var alertText = "Exception thrown. Expected: "+expected.string;
                print('<TD><font color="red"><A HREF="" '+
                      'onclick="alert(\''+alertText+'\'); return false;"'+
                      '>X</A></font></TD>');
            }
            else {
                var alertText = "Result = was wrong. Expected: "+expected.string;
                print('<TD><font color="red"><A HREF="" '+
                      'onclick="alert(\''+alertText+'\'); return false;"'+
                      '>X</A></font></TD>');
            }            
        }
        print('<TD>'+getDataTypeShortName(DataTypeArray[i])+'</TD>');
        print('</TR>');
    }

    print('</TABLE>');
}


// 

const SingleConvertResultsTableFor_String_Foo = [
/*I1,I2,I4,I8,U1,U2,U4,U8,FL,DB,BO,CH,WC,VD,ID,DS,ST,WS,NS,IF,AR,Ss,Ws,US,CS,AS,EA,EM    */
  _e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,eq,eq,eq,_e,_e,_e,eq,eq,eq,eq,eq,_e,_e
];

const SingleConvertResultsTableFor_String_5 = [
/*I1,I2,I4,I8,U1,U2,U4,U8,FL,DB,BO,CH,WC,VD,ID,DS,ST,WS,NS,IF,AR,Ss,Ws,US,CS,AS,EA,EM    */
  eq,eq,eq,eq,eq,eq,eq,eq,eq,eq,_T,NE,NE,_e,_e,eq,eq,eq,_e,_e,_e,eq,eq,eq,eq,eq,_e,_e
];

const SingleConvertResultsTableFor_Number_20 = [
/*I1,I2,I4,I8,U1,U2,U4,U8,FL,DB,BO,CH,WC,VD,ID,DS,ST,WS,NS,IF,AR,Ss,Ws,US,CS,AS,EA,EM    */
  eq,eq,eq,eq,eq,eq,eq,eq,eq,eq,_T,NE,NE,_e,_e,eq,eq,eq,_e,_e,_e,eq,eq,eq,eq,eq,_e,_e
];

const DoubleConvertResultsTableFor_String_Foo = [
/*     I1,I2,I4,I8,U1,U2,U4,U8,FL,DB,BO,CH,WC,VD,ID,DS,ST,WS,NS,IF,AR,Ss,Ws,US,CS,AS,EA,EM      */
/*I1*/[_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e],/*I1*/
/*I2*/[_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e],/*I2*/
/*I4*/[_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e],/*I4*/
/*I8*/[_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e],/*I8*/
/*U1*/[_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e],/*U1*/
/*U2*/[_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e],/*U2*/
/*U4*/[_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e],/*U4*/
/*U8*/[_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e],/*U8*/
/*FL*/[_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e],/*FL*/
/*DB*/[_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e],/*DB*/
/*BO*/[_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e],/*BO*/
/*CH*/[_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e],/*CH*/
/*WC*/[_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e],/*WC*/
/*VD*/[_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e],/*VD*/
/*ID*/[_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e],/*ID*/
/*DS*/[_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,eq,eq,eq,_e,_e,_e,eq,eq,eq,eq,eq,_e,_e],/*DS*/
/*ST*/[_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,eq,eq,eq,_e,_e,_e,eq,eq,eq,eq,eq,_e,_e],/*ST*/
/*WS*/[_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,eq,eq,eq,_e,_e,_e,eq,eq,eq,eq,eq,_e,_e],/*WS*/
/*NS*/[_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e],/*NS*/
/*IF*/[_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e],/*IF*/
/*AR*/[_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e],/*AR*/
/*Ss*/[_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,eq,eq,eq,_e,_e,_e,eq,eq,eq,eq,eq,_e,_e],/*Ss*/
/*Ws*/[_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,eq,eq,eq,_e,_e,_e,eq,eq,eq,eq,eq,_e,_e],/*Ws*/
/*US*/[_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,eq,eq,eq,_e,_e,_e,eq,eq,eq,eq,eq,_e,_e],/*US*/
/*CS*/[_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,eq,eq,eq,_e,_e,_e,eq,eq,eq,eq,eq,_e,_e],/*CS*/
/*AS*/[_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,eq,eq,eq,_e,_e,_e,eq,eq,eq,eq,eq,_e,_e],/*AS*/
/*EA*/[_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e],/*EA*/
/*EM*/[_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e] /*EM*/
];

const DoubleConvertResultsTableFor_String_Five = [
/*     I1,I2,I4,I8,U1,U2,U4,U8,FL,DB,BO,CH,WC,VD,ID,DS,ST,WS,NS,IF,AR,Ss,Ws,US,CS,AS,EA,EM      */
/*I1*/[eq,eq,eq,eq,eq,eq,eq,eq,eq,eq,_T,eq,eq,_e,_e,eq,eq,eq,_e,_e,_e,eq,eq,eq,eq,eq,_e,_e],/*I1*/
/*I2*/[eq,eq,eq,eq,eq,eq,eq,eq,eq,eq,_T,eq,eq,_e,_e,eq,eq,eq,_e,_e,_e,eq,eq,eq,eq,eq,_e,_e],/*I2*/
/*I4*/[eq,eq,eq,eq,eq,eq,eq,eq,eq,eq,_T,eq,eq,_e,_e,eq,eq,eq,_e,_e,_e,eq,eq,eq,eq,eq,_e,_e],/*I4*/
/*I8*/[eq,eq,eq,eq,eq,eq,eq,eq,eq,eq,_T,eq,eq,_e,_e,eq,eq,eq,_e,_e,_e,eq,eq,eq,eq,eq,_e,_e],/*I8*/
/*U1*/[eq,eq,eq,eq,eq,eq,eq,eq,eq,eq,_T,eq,eq,_e,_e,eq,eq,eq,_e,_e,_e,eq,eq,eq,eq,eq,_e,_e],/*U1*/
/*U2*/[eq,eq,eq,eq,eq,eq,eq,eq,eq,eq,_T,eq,eq,_e,_e,eq,eq,eq,_e,_e,_e,eq,eq,eq,eq,eq,_e,_e],/*U2*/
/*U4*/[eq,eq,eq,eq,eq,eq,eq,eq,eq,eq,_T,eq,eq,_e,_e,eq,eq,eq,_e,_e,_e,eq,eq,eq,eq,eq,_e,_e],/*U4*/
/*U8*/[eq,eq,eq,eq,eq,eq,eq,eq,eq,eq,_T,eq,eq,_e,_e,eq,eq,eq,_e,_e,_e,eq,eq,eq,eq,eq,_e,_e],/*U8*/
/*FL*/[eq,eq,eq,eq,eq,eq,eq,eq,eq,eq,_T,eq,eq,_e,_e,eq,eq,eq,_e,_e,_e,eq,eq,eq,eq,eq,_e,_e],/*FL*/
/*DB*/[eq,eq,eq,eq,eq,eq,eq,eq,eq,eq,_T,eq,eq,_e,_e,eq,eq,eq,_e,_e,_e,eq,eq,eq,eq,eq,_e,_e],/*DB*/
/*BO*/[_T,_T,_T,_T,_T,_T,_T,_T,_T,_T,_T,NE,NE,_e,_e,_T,_T,_T,_e,_e,_e,_T,_T,_T,_T,_T,_e,_e],/*BO*/
/*CH*/[NE,NE,NE,NE,NE,NE,NE,NE,NE,NE,NE,NE,NE,_e,_e,NE,NE,NE,_e,_e,_e,NE,NE,NE,NE,NE,_e,_e],/*CH*/
/*WC*/[NE,NE,NE,NE,NE,NE,NE,NE,NE,NE,NE,NE,NE,_e,_e,NE,NE,NE,_e,_e,_e,NE,NE,NE,NE,NE,_e,_e],/*WC*/
/*VD*/[_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e],/*VD*/
/*ID*/[_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e],/*ID*/
/*DS*/[eq,eq,eq,eq,eq,eq,eq,eq,eq,eq,NE,NE,NE,_e,_e,eq,eq,eq,_e,_e,_e,eq,eq,eq,eq,eq,_e,_e],/*DS*/
/*ST*/[eq,eq,eq,eq,eq,eq,eq,eq,eq,eq,NE,NE,NE,_e,_e,eq,eq,eq,_e,_e,_e,eq,eq,eq,eq,eq,_e,_e],/*ST*/
/*WS*/[eq,eq,eq,eq,eq,eq,eq,eq,eq,eq,NE,NE,NE,_e,_e,eq,eq,eq,_e,_e,_e,eq,eq,eq,eq,eq,_e,_e],/*WS*/
/*NS*/[_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e],/*NS*/
/*IF*/[_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e],/*IF*/
/*AR*/[_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e],/*AR*/
/*Ss*/[eq,eq,eq,eq,eq,eq,eq,eq,eq,eq,NE,NE,NE,_e,_e,eq,eq,eq,_e,_e,_e,eq,eq,eq,eq,eq,_e,_e],/*Ss*/
/*Ws*/[eq,eq,eq,eq,eq,eq,eq,eq,eq,eq,NE,NE,NE,_e,_e,eq,eq,eq,_e,_e,_e,eq,eq,eq,eq,eq,_e,_e],/*Ws*/
/*US*/[eq,eq,eq,eq,eq,eq,eq,eq,eq,eq,NE,NE,NE,_e,_e,eq,eq,eq,_e,_e,_e,eq,eq,eq,eq,eq,_e,_e],/*US*/
/*CS*/[eq,eq,eq,eq,eq,eq,eq,eq,eq,eq,NE,NE,NE,_e,_e,eq,eq,eq,_e,_e,_e,eq,eq,eq,eq,eq,_e,_e],/*CS*/
/*AS*/[eq,eq,eq,eq,eq,eq,eq,eq,eq,eq,NE,NE,NE,_e,_e,eq,eq,eq,_e,_e,_e,eq,eq,eq,eq,eq,_e,_e],/*AS*/
/*EA*/[_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e],/*EA*/
/*EM*/[_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e,_e] /*EM*/
];

var values = [
    "foo",
    0, 
    5, 
    1.01, 
    Components, 
    {}, 
    Components.interfaces.nsISupports,
    null,
    true,
    undefined
];

var i;

/***************************************************************************/

if(0){

print();

for(i = 0; i < values.length; i++)
    print(getDataTypeName(tv.returnVariantType(values[i])));

print();
print();

for(i = 0; i < values.length; i++)
    print(tv.passThruVariant(values[i]));

print();
print();

for(i = 0; i < values.length; i++)
    print(tv.copyVariant(values[i]));

}

function RunSingleConvertTests() {
    TestSingleConvert("foo", "a string", eqOp, SingleConvertResultsTableFor_String_Foo);
    TestSingleConvert("5", "a string", eqNumber, SingleConvertResultsTableFor_String_5);
    TestSingleConvert(20, "a number", eqNumber, SingleConvertResultsTableFor_Number_20);
    print("<P>");
}

function RunDoubleConvertTests() {
    TestDoubleConvert("foo", "a string", eqOp, DoubleConvertResultsTableFor_String_Foo);
    TestDoubleConvert("5", "a string", eqNumber, DoubleConvertResultsTableFor_String_Five);
    print("<P>");
}

// main...

print("<html><body>")

RunSingleConvertTests();
RunDoubleConvertTests();

print("</body></html>")

