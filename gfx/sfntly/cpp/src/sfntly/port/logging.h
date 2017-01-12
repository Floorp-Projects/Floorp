/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SFNTLY_CPP_SRC_SFNTLY_PORT_LOGGING_H_
#define SFNTLY_CPP_SRC_SFNTLY_PORT_LOGGING_H_

#include <stdio.h>
#include <stdlib.h>

// Cheap base/logging.h knock off.

#define CHECK(expr) \
    if (!(expr)) { \
      printf("CHECK failed\n"); \
      abort(); \
    }

#endif  // SFNTLY_CPP_SRC_SFNTLY_PORT_LOGGING_H_
