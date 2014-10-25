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

#ifndef GMP_AUDIO_HOST_h_
#define GMP_AUDIO_HOST_h_

#include "gmp-errors.h"
#include "gmp-audio-samples.h"

class GMPAudioHost
{
public:
  // Construct various Audio API objects. Host does not retain reference,
  // caller is owner and responsible for deleting.
  virtual GMPErr CreateSamples(GMPAudioFormat aFormat,
                               GMPAudioSamples** aSamples) = 0;
};

#endif // GMP_AUDIO_HOST_h_
