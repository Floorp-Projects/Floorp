package com.netscape.jsdebugging.remote.corba;
final public class IJSPC {
  public com.netscape.jsdebugging.remote.corba.IScript script;
  public int offset;
  public IJSPC() {
  }
  public IJSPC(
    com.netscape.jsdebugging.remote.corba.IScript script,
    int offset
  ) {
    this.script = script;
    this.offset = offset;
  }
  public java.lang.String toString() {
    org.omg.CORBA.Any any = org.omg.CORBA.ORB.init().create_any();
    com.netscape.jsdebugging.remote.corba.IJSPCHelper.insert(any, this);
    return any.toString();
  }
}
