/*
* Copyright 2013, Mozilla Foundation and contributors
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

#ifndef GMP_ASYNC_SHUTDOWN_H_
#define GMP_ASYNC_SHUTDOWN_H_

// API exposed by the plugin library to manage asynchronous shutdown.
// Some plugins require special cleanup which may need to make calls
// to host services and wait for async responses.
//
// To enable a plugins to block shutdown until its async shutdown is
// complete, implement the GMPAsyncShutdown interface and return it when
// your plugin's GMPGetAPI function is called with "async-shutdown".
// When your GMPAsyncShutdown's BeginShutdown() implementation is called
// by the GMP host, you should initate your async shutdown process.
// Once you have completed shutdown, call the ShutdownComplete() function
// of the GMPAsyncShutdownHost that is passed as the host argument to the
// GMPGetAPI() call.
//
// Note: Your GMP's GMPShutdown function will still be called after your
// call to ShutdownComplete().
//
// API name: "async-shutdown"
// Host API: GMPAsyncShutdownHost
class GMPAsyncShutdown {
public:
  virtual ~GMPAsyncShutdown() {}

  virtual void BeginShutdown() = 0;
};

class GMPAsyncShutdownHost {
public:
  virtual ~GMPAsyncShutdownHost() {}

  virtual void ShutdownComplete() = 0;
};

#endif // GMP_ASYNC_SHUTDOWN_H_
