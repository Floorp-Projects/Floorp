package com.netscape.jsdebugging.remote.corba;
final public class IExecResult {
  public java.lang.String result;
  public boolean errorOccured;
  public java.lang.String errorMessage;
  public java.lang.String errorFilename;
  public int errorLineNumber;
  public java.lang.String errorLineBuffer;
  public int errorTokenOffset;
  public IExecResult() {
  }
  public IExecResult(
    java.lang.String result,
    boolean errorOccured,
    java.lang.String errorMessage,
    java.lang.String errorFilename,
    int errorLineNumber,
    java.lang.String errorLineBuffer,
    int errorTokenOffset
  ) {
    this.result = result;
    this.errorOccured = errorOccured;
    this.errorMessage = errorMessage;
    this.errorFilename = errorFilename;
    this.errorLineNumber = errorLineNumber;
    this.errorLineBuffer = errorLineBuffer;
    this.errorTokenOffset = errorTokenOffset;
  }
  public java.lang.String toString() {
    org.omg.CORBA.Any any = org.omg.CORBA.ORB.init().create_any();
    com.netscape.jsdebugging.remote.corba.IExecResultHelper.insert(any, this);
    return any.toString();
  }
}
