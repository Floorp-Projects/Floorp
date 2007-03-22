
pluginhostctrlps.dll: dlldata.obj pluginhostctrl_p.obj pluginhostctrl_i.obj
	link /dll /out:pluginhostctrlps.dll /def:pluginhostctrlps.def /entry:DllMain dlldata.obj pluginhostctrl_p.obj pluginhostctrl_i.obj \
		kernel32.lib rpcndr.lib rpcns4.lib rpcrt4.lib oleaut32.lib uuid.lib \

.c.obj:
	cl /c /Ox /DWIN32 /D_WIN32_WINNT=0x0400 /DREGISTER_PROXY_DLL \
		$<

clean:
	@del pluginhostctrlps.dll
	@del pluginhostctrlps.lib
	@del pluginhostctrlps.exp
	@del dlldata.obj
	@del pluginhostctrl_p.obj
	@del pluginhostctrl_i.obj
