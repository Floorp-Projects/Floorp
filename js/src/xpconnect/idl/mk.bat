xpidl -w -m header -o ..\xpccomponents xpccomponents.idl
xpidl -w -m header -o ..\xpcjsid       xpcjsid.idl
xpidl -w -m header -o ..\xpctest       xpctest.idl

xpidl -w -m typelib -o x:\raptor\mozilla\dist\win32_d.obj\bin\xpccomponents xpccomponents.idl
xpidl -w -m typelib -o x:\raptor\mozilla\dist\win32_d.obj\bin\xpcjsid xpcjsid.idl
xpidl -w -m typelib -o x:\raptor\mozilla\dist\win32_d.obj\bin\nsISupports nsISupports.idl
xpidl -w -m typelib -o x:\raptor\mozilla\dist\win32_d.obj\bin\xpctest xpctest.idl
set XPTDIR=x:\raptor\mozilla\dist\win32_d.obj\bin