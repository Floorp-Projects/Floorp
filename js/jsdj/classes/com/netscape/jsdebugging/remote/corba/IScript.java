package com.netscape.jsdebugging.remote.corba;
final public class IScript {
  public java.lang.String url;
  public java.lang.String funname;
  public int base;
  public int extent;
  public int jsdscript;
  public com.netscape.jsdebugging.remote.corba.IScriptSection[] sections;
  public IScript() {
  }
  public IScript(
    java.lang.String url,
    java.lang.String funname,
    int base,
    int extent,
    int jsdscript,
    com.netscape.jsdebugging.remote.corba.IScriptSection[] sections
  ) {
    this.url = url;
    this.funname = funname;
    this.base = base;
    this.extent = extent;
    this.jsdscript = jsdscript;
    this.sections = sections;
  }
  public java.lang.String toString() {
    org.omg.CORBA.Any any = org.omg.CORBA.ORB.init().create_any();
    com.netscape.jsdebugging.remote.corba.IScriptHelper.insert(any, this);
    return any.toString();
  }
}
