package com.netscape.jsdebugging.remote.corba;
final public class IScriptSection {
  public int base;
  public int extent;
  public IScriptSection() {
  }
  public IScriptSection(
    int base,
    int extent
  ) {
    this.base = base;
    this.extent = extent;
  }
  public java.lang.String toString() {
    org.omg.CORBA.Any any = org.omg.CORBA.ORB.init().create_any();
    com.netscape.jsdebugging.remote.corba.IScriptSectionHelper.insert(any, this);
    return any.toString();
  }
}
