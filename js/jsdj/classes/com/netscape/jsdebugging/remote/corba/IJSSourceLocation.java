package com.netscape.jsdebugging.remote.corba;
final public class IJSSourceLocation {
  public int line;
  public com.netscape.jsdebugging.remote.corba.IJSPC pc;
  public IJSSourceLocation() {
  }
  public IJSSourceLocation(
    int line,
    com.netscape.jsdebugging.remote.corba.IJSPC pc
  ) {
    this.line = line;
    this.pc = pc;
  }
  public java.lang.String toString() {
    org.omg.CORBA.Any any = org.omg.CORBA.ORB.init().create_any();
    com.netscape.jsdebugging.remote.corba.IJSSourceLocationHelper.insert(any, this);
    return any.toString();
  }
}
