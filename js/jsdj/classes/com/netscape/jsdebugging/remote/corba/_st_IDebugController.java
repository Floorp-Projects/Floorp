package com.netscape.jsdebugging.remote.corba;
public class _st_IDebugController extends org.omg.CORBA.portable.ObjectImpl implements com.netscape.jsdebugging.remote.corba.IDebugController {
  public java.lang.String[] _ids() {
    return __ids;
  }
  private static java.lang.String[] __ids = {
    "IDL:IDebugController:1.0"
  };
  public int getMajorVersion() {
    try {
      org.omg.CORBA.portable.OutputStream _output = this._request("getMajorVersion", true);
      org.omg.CORBA.portable.InputStream _input = this._invoke(_output, null);
      int _result;
      _result = _input.read_long();
      return _result;
    }
    catch(org.omg.CORBA.TRANSIENT _exception) {
      return getMajorVersion();
    }
  }
  public int getMinorVersion() {
    try {
      org.omg.CORBA.portable.OutputStream _output = this._request("getMinorVersion", true);
      org.omg.CORBA.portable.InputStream _input = this._invoke(_output, null);
      int _result;
      _result = _input.read_long();
      return _result;
    }
    catch(org.omg.CORBA.TRANSIENT _exception) {
      return getMinorVersion();
    }
  }
  public com.netscape.jsdebugging.remote.corba.IJSErrorReporter setErrorReporter(
    com.netscape.jsdebugging.remote.corba.IJSErrorReporter arg0
  ) {
    try {
      org.omg.CORBA.portable.OutputStream _output = this._request("setErrorReporter", true);
      com.netscape.jsdebugging.remote.corba.IJSErrorReporterHelper.write(_output, arg0);
      org.omg.CORBA.portable.InputStream _input = this._invoke(_output, null);
      com.netscape.jsdebugging.remote.corba.IJSErrorReporter _result;
      _result = com.netscape.jsdebugging.remote.corba.IJSErrorReporterHelper.read(_input);
      return _result;
    }
    catch(org.omg.CORBA.TRANSIENT _exception) {
      return setErrorReporter(
        arg0
      );
    }
  }
  public com.netscape.jsdebugging.remote.corba.IJSErrorReporter getErrorReporter() {
    try {
      org.omg.CORBA.portable.OutputStream _output = this._request("getErrorReporter", true);
      org.omg.CORBA.portable.InputStream _input = this._invoke(_output, null);
      com.netscape.jsdebugging.remote.corba.IJSErrorReporter _result;
      _result = com.netscape.jsdebugging.remote.corba.IJSErrorReporterHelper.read(_input);
      return _result;
    }
    catch(org.omg.CORBA.TRANSIENT _exception) {
      return getErrorReporter();
    }
  }
  public com.netscape.jsdebugging.remote.corba.IScriptHook setScriptHook(
    com.netscape.jsdebugging.remote.corba.IScriptHook arg0
  ) {
    try {
      org.omg.CORBA.portable.OutputStream _output = this._request("setScriptHook", true);
      com.netscape.jsdebugging.remote.corba.IScriptHookHelper.write(_output, arg0);
      org.omg.CORBA.portable.InputStream _input = this._invoke(_output, null);
      com.netscape.jsdebugging.remote.corba.IScriptHook _result;
      _result = com.netscape.jsdebugging.remote.corba.IScriptHookHelper.read(_input);
      return _result;
    }
    catch(org.omg.CORBA.TRANSIENT _exception) {
      return setScriptHook(
        arg0
      );
    }
  }
  public com.netscape.jsdebugging.remote.corba.IScriptHook getScriptHook() {
    try {
      org.omg.CORBA.portable.OutputStream _output = this._request("getScriptHook", true);
      org.omg.CORBA.portable.InputStream _input = this._invoke(_output, null);
      com.netscape.jsdebugging.remote.corba.IScriptHook _result;
      _result = com.netscape.jsdebugging.remote.corba.IScriptHookHelper.read(_input);
      return _result;
    }
    catch(org.omg.CORBA.TRANSIENT _exception) {
      return getScriptHook();
    }
  }
  public com.netscape.jsdebugging.remote.corba.IJSPC getClosestPC(
    com.netscape.jsdebugging.remote.corba.IScript arg0,
    int arg1
  ) {
    try {
      org.omg.CORBA.portable.OutputStream _output = this._request("getClosestPC", true);
      com.netscape.jsdebugging.remote.corba.IScriptHelper.write(_output, arg0);
      _output.write_long(arg1);
      org.omg.CORBA.portable.InputStream _input = this._invoke(_output, null);
      com.netscape.jsdebugging.remote.corba.IJSPC _result;
      _result = com.netscape.jsdebugging.remote.corba.IJSPCHelper.read(_input);
      return _result;
    }
    catch(org.omg.CORBA.TRANSIENT _exception) {
      return getClosestPC(
        arg0,
        arg1
      );
    }
  }
  public com.netscape.jsdebugging.remote.corba.IJSSourceLocation getSourceLocation(
    com.netscape.jsdebugging.remote.corba.IJSPC arg0
  ) {
    try {
      org.omg.CORBA.portable.OutputStream _output = this._request("getSourceLocation", true);
      com.netscape.jsdebugging.remote.corba.IJSPCHelper.write(_output, arg0);
      org.omg.CORBA.portable.InputStream _input = this._invoke(_output, null);
      com.netscape.jsdebugging.remote.corba.IJSSourceLocation _result;
      _result = com.netscape.jsdebugging.remote.corba.IJSSourceLocationHelper.read(_input);
      return _result;
    }
    catch(org.omg.CORBA.TRANSIENT _exception) {
      return getSourceLocation(
        arg0
      );
    }
  }
  public com.netscape.jsdebugging.remote.corba.IJSExecutionHook setInterruptHook(
    com.netscape.jsdebugging.remote.corba.IJSExecutionHook arg0
  ) {
    try {
      org.omg.CORBA.portable.OutputStream _output = this._request("setInterruptHook", true);
      com.netscape.jsdebugging.remote.corba.IJSExecutionHookHelper.write(_output, arg0);
      org.omg.CORBA.portable.InputStream _input = this._invoke(_output, null);
      com.netscape.jsdebugging.remote.corba.IJSExecutionHook _result;
      _result = com.netscape.jsdebugging.remote.corba.IJSExecutionHookHelper.read(_input);
      return _result;
    }
    catch(org.omg.CORBA.TRANSIENT _exception) {
      return setInterruptHook(
        arg0
      );
    }
  }
  public com.netscape.jsdebugging.remote.corba.IJSExecutionHook getInterruptHook() {
    try {
      org.omg.CORBA.portable.OutputStream _output = this._request("getInterruptHook", true);
      org.omg.CORBA.portable.InputStream _input = this._invoke(_output, null);
      com.netscape.jsdebugging.remote.corba.IJSExecutionHook _result;
      _result = com.netscape.jsdebugging.remote.corba.IJSExecutionHookHelper.read(_input);
      return _result;
    }
    catch(org.omg.CORBA.TRANSIENT _exception) {
      return getInterruptHook();
    }
  }
  public com.netscape.jsdebugging.remote.corba.IJSExecutionHook setDebugBreakHook(
    com.netscape.jsdebugging.remote.corba.IJSExecutionHook arg0
  ) {
    try {
      org.omg.CORBA.portable.OutputStream _output = this._request("setDebugBreakHook", true);
      com.netscape.jsdebugging.remote.corba.IJSExecutionHookHelper.write(_output, arg0);
      org.omg.CORBA.portable.InputStream _input = this._invoke(_output, null);
      com.netscape.jsdebugging.remote.corba.IJSExecutionHook _result;
      _result = com.netscape.jsdebugging.remote.corba.IJSExecutionHookHelper.read(_input);
      return _result;
    }
    catch(org.omg.CORBA.TRANSIENT _exception) {
      return setDebugBreakHook(
        arg0
      );
    }
  }
  public com.netscape.jsdebugging.remote.corba.IJSExecutionHook getDebugBreakHook() {
    try {
      org.omg.CORBA.portable.OutputStream _output = this._request("getDebugBreakHook", true);
      org.omg.CORBA.portable.InputStream _input = this._invoke(_output, null);
      com.netscape.jsdebugging.remote.corba.IJSExecutionHook _result;
      _result = com.netscape.jsdebugging.remote.corba.IJSExecutionHookHelper.read(_input);
      return _result;
    }
    catch(org.omg.CORBA.TRANSIENT _exception) {
      return getDebugBreakHook();
    }
  }
  public com.netscape.jsdebugging.remote.corba.IJSExecutionHook setInstructionHook(
    com.netscape.jsdebugging.remote.corba.IJSExecutionHook arg0,
    com.netscape.jsdebugging.remote.corba.IJSPC arg1
  ) {
    try {
      org.omg.CORBA.portable.OutputStream _output = this._request("setInstructionHook", true);
      com.netscape.jsdebugging.remote.corba.IJSExecutionHookHelper.write(_output, arg0);
      com.netscape.jsdebugging.remote.corba.IJSPCHelper.write(_output, arg1);
      org.omg.CORBA.portable.InputStream _input = this._invoke(_output, null);
      com.netscape.jsdebugging.remote.corba.IJSExecutionHook _result;
      _result = com.netscape.jsdebugging.remote.corba.IJSExecutionHookHelper.read(_input);
      return _result;
    }
    catch(org.omg.CORBA.TRANSIENT _exception) {
      return setInstructionHook(
        arg0,
        arg1
      );
    }
  }
  public com.netscape.jsdebugging.remote.corba.IJSExecutionHook getInstructionHook(
    com.netscape.jsdebugging.remote.corba.IJSPC arg0
  ) {
    try {
      org.omg.CORBA.portable.OutputStream _output = this._request("getInstructionHook", true);
      com.netscape.jsdebugging.remote.corba.IJSPCHelper.write(_output, arg0);
      org.omg.CORBA.portable.InputStream _input = this._invoke(_output, null);
      com.netscape.jsdebugging.remote.corba.IJSExecutionHook _result;
      _result = com.netscape.jsdebugging.remote.corba.IJSExecutionHookHelper.read(_input);
      return _result;
    }
    catch(org.omg.CORBA.TRANSIENT _exception) {
      return getInstructionHook(
        arg0
      );
    }
  }
  public void setThreadContinueState(
    int arg0,
    int arg1
  ) {
    try {
      org.omg.CORBA.portable.OutputStream _output = this._request("setThreadContinueState", true);
      _output.write_long(arg0);
      _output.write_long(arg1);
      org.omg.CORBA.portable.InputStream _input = this._invoke(_output, null);
    }
    catch(org.omg.CORBA.TRANSIENT _exception) {
      setThreadContinueState(
        arg0,
        arg1
      );
    }
  }
  public void setThreadReturnValue(
    int arg0,
    java.lang.String arg1
  ) {
    try {
      org.omg.CORBA.portable.OutputStream _output = this._request("setThreadReturnValue", true);
      _output.write_long(arg0);
      _output.write_string(arg1);
      org.omg.CORBA.portable.InputStream _input = this._invoke(_output, null);
    }
    catch(org.omg.CORBA.TRANSIENT _exception) {
      setThreadReturnValue(
        arg0,
        arg1
      );
    }
  }
  public void sendInterrupt() {
    try {
      org.omg.CORBA.portable.OutputStream _output = this._request("sendInterrupt", true);
      org.omg.CORBA.portable.InputStream _input = this._invoke(_output, null);
    }
    catch(org.omg.CORBA.TRANSIENT _exception) {
      sendInterrupt();
    }
  }
  public void sendInterruptStepInto(
    int arg0
  ) {
    try {
      org.omg.CORBA.portable.OutputStream _output = this._request("sendInterruptStepInto", true);
      _output.write_long(arg0);
      org.omg.CORBA.portable.InputStream _input = this._invoke(_output, null);
    }
    catch(org.omg.CORBA.TRANSIENT _exception) {
      sendInterruptStepInto(
        arg0
      );
    }
  }
  public void sendInterruptStepOver(
    int arg0
  ) {
    try {
      org.omg.CORBA.portable.OutputStream _output = this._request("sendInterruptStepOver", true);
      _output.write_long(arg0);
      org.omg.CORBA.portable.InputStream _input = this._invoke(_output, null);
    }
    catch(org.omg.CORBA.TRANSIENT _exception) {
      sendInterruptStepOver(
        arg0
      );
    }
  }
  public void sendInterruptStepOut(
    int arg0
  ) {
    try {
      org.omg.CORBA.portable.OutputStream _output = this._request("sendInterruptStepOut", true);
      _output.write_long(arg0);
      org.omg.CORBA.portable.InputStream _input = this._invoke(_output, null);
    }
    catch(org.omg.CORBA.TRANSIENT _exception) {
      sendInterruptStepOut(
        arg0
      );
    }
  }
  public void reinstateStepper(
    int arg0
  ) {
    try {
      org.omg.CORBA.portable.OutputStream _output = this._request("reinstateStepper", true);
      _output.write_long(arg0);
      org.omg.CORBA.portable.InputStream _input = this._invoke(_output, null);
    }
    catch(org.omg.CORBA.TRANSIENT _exception) {
      reinstateStepper(
        arg0
      );
    }
  }
  public com.netscape.jsdebugging.remote.corba.IExecResult executeScriptInStackFrame(
    int arg0,
    com.netscape.jsdebugging.remote.corba.IJSStackFrameInfo arg1,
    java.lang.String arg2,
    java.lang.String arg3,
    int arg4
  ) {
    try {
      org.omg.CORBA.portable.OutputStream _output = this._request("executeScriptInStackFrame", true);
      _output.write_long(arg0);
      com.netscape.jsdebugging.remote.corba.IJSStackFrameInfoHelper.write(_output, arg1);
      _output.write_string(arg2);
      _output.write_string(arg3);
      _output.write_long(arg4);
      org.omg.CORBA.portable.InputStream _input = this._invoke(_output, null);
      com.netscape.jsdebugging.remote.corba.IExecResult _result;
      _result = com.netscape.jsdebugging.remote.corba.IExecResultHelper.read(_input);
      return _result;
    }
    catch(org.omg.CORBA.TRANSIENT _exception) {
      return executeScriptInStackFrame(
        arg0,
        arg1,
        arg2,
        arg3,
        arg4
      );
    }
  }
  public boolean isRunningHook(
    int arg0
  ) {
    try {
      org.omg.CORBA.portable.OutputStream _output = this._request("isRunningHook", true);
      _output.write_long(arg0);
      org.omg.CORBA.portable.InputStream _input = this._invoke(_output, null);
      boolean _result;
      _result = _input.read_boolean();
      return _result;
    }
    catch(org.omg.CORBA.TRANSIENT _exception) {
      return isRunningHook(
        arg0
      );
    }
  }
  public boolean isWaitingForResume(
    int arg0
  ) {
    try {
      org.omg.CORBA.portable.OutputStream _output = this._request("isWaitingForResume", true);
      _output.write_long(arg0);
      org.omg.CORBA.portable.InputStream _input = this._invoke(_output, null);
      boolean _result;
      _result = _input.read_boolean();
      return _result;
    }
    catch(org.omg.CORBA.TRANSIENT _exception) {
      return isWaitingForResume(
        arg0
      );
    }
  }
  public void leaveThreadSuspended(
    int arg0
  ) {
    try {
      org.omg.CORBA.portable.OutputStream _output = this._request("leaveThreadSuspended", true);
      _output.write_long(arg0);
      org.omg.CORBA.portable.InputStream _input = this._invoke(_output, null);
    }
    catch(org.omg.CORBA.TRANSIENT _exception) {
      leaveThreadSuspended(
        arg0
      );
    }
  }
  public void resumeThread(
    int arg0
  ) {
    try {
      org.omg.CORBA.portable.OutputStream _output = this._request("resumeThread", true);
      _output.write_long(arg0);
      org.omg.CORBA.portable.InputStream _input = this._invoke(_output, null);
    }
    catch(org.omg.CORBA.TRANSIENT _exception) {
      resumeThread(
        arg0
      );
    }
  }
  public void iterateScripts(
    com.netscape.jsdebugging.remote.corba.IScriptHook arg0
  ) {
    try {
      org.omg.CORBA.portable.OutputStream _output = this._request("iterateScripts", true);
      com.netscape.jsdebugging.remote.corba.IScriptHookHelper.write(_output, arg0);
      org.omg.CORBA.portable.InputStream _input = this._invoke(_output, null);
    }
    catch(org.omg.CORBA.TRANSIENT _exception) {
      iterateScripts(
        arg0
      );
    }
  }
}
