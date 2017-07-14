/*
   Copyright 2014-2015 Stanislav Ovsyannikov

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

	   Unless required by applicable law or agreed to in writing, software
	   distributed under the License is distributed on an "AS IS" BASIS,
	   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
	   See the License for the specific language governing permissions and
	   limitations under the License.
*/


#ifndef __CborEncoder_H_
#define __CborEncoder_H_

#include "output.h"
#include <string>

namespace cbor {
    class encoder {
    private:
        output *_out;
    public:
        explicit encoder(output &out);

        ~encoder();

        void write_bool(bool value);

        void write_int(int value);

        void write_int(long long value);

        void write_int(unsigned int value);

        void write_int(unsigned long long value);

        void write_bytes(const unsigned char *data, unsigned int size);

        void write_string(const char *data, unsigned int size);

        void write_string(const std::string str);

        void write_array(int size);

        void write_map(int size);

        void write_tag(const unsigned int tag);

        void write_special(int special);

        void write_null();

        void write_undefined();

    private:
        void write_type_value(int major_type, unsigned int value);

        void write_type_value(int major_type, unsigned long long value);
    };
}

#endif //__CborEncoder_H_
