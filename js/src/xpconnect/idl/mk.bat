
xpidl -w -m header -o ..\public\xpccomponents       xpccomponents.idl
xpidl -w -m header -o ..\public\xpcjsid             xpcjsid.idl
xpidl -w -m header -o ..\tests\xpctest              xpctest.idl

xpidl -w -m typelib -o ..\typelib\xpccomponents     xpccomponents.idl
xpidl -w -m typelib -o ..\typelib\xpcjsid           xpcjsid.idl
xpidl -w -m typelib -o ..\typelib\nsISupports       nsISupports.idl
xpidl -w -m typelib -o ..\typelib\xpctest           xpctest.idl


rem set XPTDIR=x:\raptor\mozilla\js\src\xpconnect\typelib
