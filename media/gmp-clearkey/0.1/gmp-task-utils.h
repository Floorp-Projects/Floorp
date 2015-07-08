/*
 * Copyright 2015, Mozilla Foundation and contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Original author: ekr@rtfm.com

#ifndef gmp_task_utils_h_
#define gmp_task_utils_h_

#include "gmp-api/gmp-platform.h"

class gmp_task_args_base : public GMPTask {
public:
  virtual void Destroy() { delete this; }
  virtual void Run() = 0;
};

// The generated file contains four major function templates
// (in variants for arbitrary numbers of arguments up to 10,
// which is why it is machine generated). The four templates
// are:
//
// WrapTask(o, m, ...) -- wraps a member function m of an object ptr o
// WrapTaskRet(o, m, ..., r) -- wraps a member function m of an object ptr o
//                                  the function returns something that can
//                                  be assigned to *r
// WrapTaskNM(f, ...) -- wraps a function f
// WrapTaskNMRet(f, ..., r) -- wraps a function f that returns something
//                                 that can be assigned to *r
//
// All of these template functions return a GMPTask* which can be passed
// to DispatchXX().
#include "gmp-task-utils-generated.h"

#endif // gmp_task_utils_h_
