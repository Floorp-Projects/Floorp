package com.netscape.jsdebugging.remote.corba;
public interface IScriptHook extends org.omg.CORBA.Object {
  public void justLoadedScript(
    com.netscape.jsdebugging.remote.corba.IScript arg0
  );
  public void aboutToUnloadScript(
    com.netscape.jsdebugging.remote.corba.IScript arg0
  );
}
