package com.netscape.jsdebugging.remote.corba;
final public class IJSStackFrameInfo {
  public com.netscape.jsdebugging.remote.corba.IJSPC pc;
  public int jsdframe;
  public IJSStackFrameInfo() {
  }
  public IJSStackFrameInfo(
    com.netscape.jsdebugging.remote.corba.IJSPC pc,
    int jsdframe
  ) {
    this.pc = pc;
    this.jsdframe = jsdframe;
  }
  public java.lang.String toString() {
    org.omg.CORBA.Any any = org.omg.CORBA.ORB.init().create_any();
    com.netscape.jsdebugging.remote.corba.IJSStackFrameInfoHelper.insert(any, this);
    return any.toString();
  }
}
