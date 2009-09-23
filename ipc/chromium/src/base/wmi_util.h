// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// WMI (Windows Management and Instrumentation) is a big, complex, COM-based
// API that can be used to perform all sorts of things. Sometimes is the best
// way to accomplish something under windows but its lack of an approachable
// C++ interface prevents its use. This collection of fucntions is a step in
// that direction.
// There are two classes; WMIUtil and WMIProcessUtil. The first
// one contain generic helpers and the second one contains the only
// functionality that is needed right now which is to use WMI to launch a
// process.
// To use any function on this header you must call CoInitialize or
// CoInitializeEx beforehand.
//
// For more information about WMI programming:
// http://msdn2.microsoft.com/en-us/library/aa384642(VS.85).aspx

#ifndef BASE_WMI_UTIL_H__
#define BASE_WMI_UTIL_H__

#include <string>
#include <wbemidl.h>

class WMIUtil {
 public:
  // Creates an instance of the WMI service connected to the local computer and
  // returns its COM interface. If 'set-blanket' is set to true, the basic COM
  // security blanket is applied to the returned interface. This is almost
  // always desirable unless you set the parameter to false and apply a custom
  // COM security blanket.
  // Returns true if succeeded and 'wmi_services': the pointer to the service.
  // When done with the interface you must call Release();
  static bool CreateLocalConnection(bool set_blanket,
                                    IWbemServices** wmi_services);

  // Creates a WMI method using from a WMI class named 'class_name' that
  // contains a method named 'method_name'. Only WMI classes that are CIM
  // classes can be created using this function.
  // Returns true if succeeded and 'class_instance' returns a pointer to the
  // WMI method that you can fill with parameter values using SetParameter.
  // When done with the interface you must call Release();
  static bool CreateClassMethodObject(IWbemServices* wmi_services,
                                      const std::wstring& class_name,
                                      const std::wstring& method_name,
                                      IWbemClassObject** class_instance);

  // Fills a single parameter given an instanced 'class_method'. Returns true
  // if operation succeeded. When all the parameters are set the method can
  // be executed using IWbemServices::ExecMethod().
  static bool SetParameter(IWbemClassObject* class_method,
                           const std::wstring& parameter_name,
                           VARIANT* parameter);
};

// This class contains functionality of the WMI class 'Win32_Process'
// more info: http://msdn2.microsoft.com/en-us/library/aa394372(VS.85).aspx
class WMIProcessUtil {
 public:
  // Creates a new process from 'command_line'. The advantage over CreateProcess
  // is that it allows you to always break out from a Job object that the caller
  // is attached to even if the Job object flags prevent that.
  // Returns true and the process id in process_id if the process is launched
  // successful. False otherwise.
  // Note that a fully qualified path must be specified in most cases unless
  // the program is not in the search path of winmgmt.exe.
  // Processes created this way are children of wmiprvse.exe and run with the
  // caller credentials.
  static bool Launch(const std::wstring& command_line, int* process_id);
};

#endif  // BASE_WMI_UTIL_H__
