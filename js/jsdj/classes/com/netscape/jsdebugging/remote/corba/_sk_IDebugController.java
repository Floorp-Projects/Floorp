package com.netscape.jsdebugging.remote.corba;
abstract public class _sk_IDebugController extends org.omg.CORBA.portable.Skeleton implements com.netscape.jsdebugging.remote.corba.IDebugController {
  protected _sk_IDebugController(java.lang.String name) {
    super(name);
  }
  protected _sk_IDebugController() {
    super();
  }
  public java.lang.String[] _ids() {
    return __ids;
  }
  private static java.lang.String[] __ids = {
    "IDL:IDebugController:1.0"
  };
  public org.omg.CORBA.portable.MethodPointer[] _methods() {
    org.omg.CORBA.portable.MethodPointer[] methods = {
      new org.omg.CORBA.portable.MethodPointer("getMajorVersion", 0, 0),
      new org.omg.CORBA.portable.MethodPointer("getMinorVersion", 0, 1),
      new org.omg.CORBA.portable.MethodPointer("setErrorReporter", 0, 2),
      new org.omg.CORBA.portable.MethodPointer("getErrorReporter", 0, 3),
      new org.omg.CORBA.portable.MethodPointer("setScriptHook", 0, 4),
      new org.omg.CORBA.portable.MethodPointer("getScriptHook", 0, 5),
      new org.omg.CORBA.portable.MethodPointer("getClosestPC", 0, 6),
      new org.omg.CORBA.portable.MethodPointer("getSourceLocation", 0, 7),
      new org.omg.CORBA.portable.MethodPointer("setInterruptHook", 0, 8),
      new org.omg.CORBA.portable.MethodPointer("getInterruptHook", 0, 9),
      new org.omg.CORBA.portable.MethodPointer("setDebugBreakHook", 0, 10),
      new org.omg.CORBA.portable.MethodPointer("getDebugBreakHook", 0, 11),
      new org.omg.CORBA.portable.MethodPointer("setInstructionHook", 0, 12),
      new org.omg.CORBA.portable.MethodPointer("getInstructionHook", 0, 13),
      new org.omg.CORBA.portable.MethodPointer("setThreadContinueState", 0, 14),
      new org.omg.CORBA.portable.MethodPointer("setThreadReturnValue", 0, 15),
      new org.omg.CORBA.portable.MethodPointer("sendInterrupt", 0, 16),
      new org.omg.CORBA.portable.MethodPointer("sendInterruptStepInto", 0, 17),
      new org.omg.CORBA.portable.MethodPointer("sendInterruptStepOver", 0, 18),
      new org.omg.CORBA.portable.MethodPointer("sendInterruptStepOut", 0, 19),
      new org.omg.CORBA.portable.MethodPointer("reinstateStepper", 0, 20),
      new org.omg.CORBA.portable.MethodPointer("executeScriptInStackFrame", 0, 21),
      new org.omg.CORBA.portable.MethodPointer("isRunningHook", 0, 22),
      new org.omg.CORBA.portable.MethodPointer("isWaitingForResume", 0, 23),
      new org.omg.CORBA.portable.MethodPointer("leaveThreadSuspended", 0, 24),
      new org.omg.CORBA.portable.MethodPointer("resumeThread", 0, 25),
      new org.omg.CORBA.portable.MethodPointer("iterateScripts", 0, 26),
    };
    return methods;
  }
  public boolean _execute(org.omg.CORBA.portable.MethodPointer method, org.omg.CORBA.portable.InputStream input, org.omg.CORBA.portable.OutputStream output) {
    switch(method.interface_id) {
    case 0: {
      return com.netscape.jsdebugging.remote.corba._sk_IDebugController._execute(this, method.method_id, input, output); 
    }
    }
    throw new org.omg.CORBA.MARSHAL();
  }
  public static boolean _execute(com.netscape.jsdebugging.remote.corba.IDebugController _self, int _method_id, org.omg.CORBA.portable.InputStream _input, org.omg.CORBA.portable.OutputStream _output) {
    switch(_method_id) {
    case 0: {
      int _result = _self.getMajorVersion();
      _output.write_long(_result);
      return false;
    }
    case 1: {
      int _result = _self.getMinorVersion();
      _output.write_long(_result);
      return false;
    }
    case 2: {
      com.netscape.jsdebugging.remote.corba.IJSErrorReporter arg0;
      arg0 = com.netscape.jsdebugging.remote.corba.IJSErrorReporterHelper.read(_input);
      com.netscape.jsdebugging.remote.corba.IJSErrorReporter _result = _self.setErrorReporter(arg0);
      com.netscape.jsdebugging.remote.corba.IJSErrorReporterHelper.write(_output, _result);
      return false;
    }
    case 3: {
      com.netscape.jsdebugging.remote.corba.IJSErrorReporter _result = _self.getErrorReporter();
      com.netscape.jsdebugging.remote.corba.IJSErrorReporterHelper.write(_output, _result);
      return false;
    }
    case 4: {
      com.netscape.jsdebugging.remote.corba.IScriptHook arg0;
      arg0 = com.netscape.jsdebugging.remote.corba.IScriptHookHelper.read(_input);
      com.netscape.jsdebugging.remote.corba.IScriptHook _result = _self.setScriptHook(arg0);
      com.netscape.jsdebugging.remote.corba.IScriptHookHelper.write(_output, _result);
      return false;
    }
    case 5: {
      com.netscape.jsdebugging.remote.corba.IScriptHook _result = _self.getScriptHook();
      com.netscape.jsdebugging.remote.corba.IScriptHookHelper.write(_output, _result);
      return false;
    }
    case 6: {
      com.netscape.jsdebugging.remote.corba.IScript arg0;
      arg0 = com.netscape.jsdebugging.remote.corba.IScriptHelper.read(_input);
      int arg1;
      arg1 = _input.read_long();
      com.netscape.jsdebugging.remote.corba.IJSPC _result = _self.getClosestPC(arg0,arg1);
      com.netscape.jsdebugging.remote.corba.IJSPCHelper.write(_output, _result);
      return false;
    }
    case 7: {
      com.netscape.jsdebugging.remote.corba.IJSPC arg0;
      arg0 = com.netscape.jsdebugging.remote.corba.IJSPCHelper.read(_input);
      com.netscape.jsdebugging.remote.corba.IJSSourceLocation _result = _self.getSourceLocation(arg0);
      com.netscape.jsdebugging.remote.corba.IJSSourceLocationHelper.write(_output, _result);
      return false;
    }
    case 8: {
      com.netscape.jsdebugging.remote.corba.IJSExecutionHook arg0;
      arg0 = com.netscape.jsdebugging.remote.corba.IJSExecutionHookHelper.read(_input);
      com.netscape.jsdebugging.remote.corba.IJSExecutionHook _result = _self.setInterruptHook(arg0);
      com.netscape.jsdebugging.remote.corba.IJSExecutionHookHelper.write(_output, _result);
      return false;
    }
    case 9: {
      com.netscape.jsdebugging.remote.corba.IJSExecutionHook _result = _self.getInterruptHook();
      com.netscape.jsdebugging.remote.corba.IJSExecutionHookHelper.write(_output, _result);
      return false;
    }
    case 10: {
      com.netscape.jsdebugging.remote.corba.IJSExecutionHook arg0;
      arg0 = com.netscape.jsdebugging.remote.corba.IJSExecutionHookHelper.read(_input);
      com.netscape.jsdebugging.remote.corba.IJSExecutionHook _result = _self.setDebugBreakHook(arg0);
      com.netscape.jsdebugging.remote.corba.IJSExecutionHookHelper.write(_output, _result);
      return false;
    }
    case 11: {
      com.netscape.jsdebugging.remote.corba.IJSExecutionHook _result = _self.getDebugBreakHook();
      com.netscape.jsdebugging.remote.corba.IJSExecutionHookHelper.write(_output, _result);
      return false;
    }
    case 12: {
      com.netscape.jsdebugging.remote.corba.IJSExecutionHook arg0;
      arg0 = com.netscape.jsdebugging.remote.corba.IJSExecutionHookHelper.read(_input);
      com.netscape.jsdebugging.remote.corba.IJSPC arg1;
      arg1 = com.netscape.jsdebugging.remote.corba.IJSPCHelper.read(_input);
      com.netscape.jsdebugging.remote.corba.IJSExecutionHook _result = _self.setInstructionHook(arg0,arg1);
      com.netscape.jsdebugging.remote.corba.IJSExecutionHookHelper.write(_output, _result);
      return false;
    }
    case 13: {
      com.netscape.jsdebugging.remote.corba.IJSPC arg0;
      arg0 = com.netscape.jsdebugging.remote.corba.IJSPCHelper.read(_input);
      com.netscape.jsdebugging.remote.corba.IJSExecutionHook _result = _self.getInstructionHook(arg0);
      com.netscape.jsdebugging.remote.corba.IJSExecutionHookHelper.write(_output, _result);
      return false;
    }
    case 14: {
      int arg0;
      arg0 = _input.read_long();
      int arg1;
      arg1 = _input.read_long();
      _self.setThreadContinueState(arg0,arg1);
      return false;
    }
    case 15: {
      int arg0;
      arg0 = _input.read_long();
      java.lang.String arg1;
      arg1 = _input.read_string();
      _self.setThreadReturnValue(arg0,arg1);
      return false;
    }
    case 16: {
      _self.sendInterrupt();
      return false;
    }
    case 17: {
      int arg0;
      arg0 = _input.read_long();
      _self.sendInterruptStepInto(arg0);
      return false;
    }
    case 18: {
      int arg0;
      arg0 = _input.read_long();
      _self.sendInterruptStepOver(arg0);
      return false;
    }
    case 19: {
      int arg0;
      arg0 = _input.read_long();
      _self.sendInterruptStepOut(arg0);
      return false;
    }
    case 20: {
      int arg0;
      arg0 = _input.read_long();
      _self.reinstateStepper(arg0);
      return false;
    }
    case 21: {
      int arg0;
      arg0 = _input.read_long();
      com.netscape.jsdebugging.remote.corba.IJSStackFrameInfo arg1;
      arg1 = com.netscape.jsdebugging.remote.corba.IJSStackFrameInfoHelper.read(_input);
      java.lang.String arg2;
      arg2 = _input.read_string();
      java.lang.String arg3;
      arg3 = _input.read_string();
      int arg4;
      arg4 = _input.read_long();
      com.netscape.jsdebugging.remote.corba.IExecResult _result = _self.executeScriptInStackFrame(arg0,arg1,arg2,arg3,arg4);
      com.netscape.jsdebugging.remote.corba.IExecResultHelper.write(_output, _result);
      return false;
    }
    case 22: {
      int arg0;
      arg0 = _input.read_long();
      boolean _result = _self.isRunningHook(arg0);
      _output.write_boolean(_result);
      return false;
    }
    case 23: {
      int arg0;
      arg0 = _input.read_long();
      boolean _result = _self.isWaitingForResume(arg0);
      _output.write_boolean(_result);
      return false;
    }
    case 24: {
      int arg0;
      arg0 = _input.read_long();
      _self.leaveThreadSuspended(arg0);
      return false;
    }
    case 25: {
      int arg0;
      arg0 = _input.read_long();
      _self.resumeThread(arg0);
      return false;
    }
    case 26: {
      com.netscape.jsdebugging.remote.corba.IScriptHook arg0;
      arg0 = com.netscape.jsdebugging.remote.corba.IScriptHookHelper.read(_input);
      _self.iterateScripts(arg0);
      return false;
    }
    }
    throw new org.omg.CORBA.MARSHAL();
  }
}
