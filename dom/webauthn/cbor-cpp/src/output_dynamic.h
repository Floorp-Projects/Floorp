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

#ifndef __CborDynamicOutput_H_
#define __CborDynamicOutput_H_


#include "output.h"

namespace cbor {
    class output_dynamic : public output {
    private:
        unsigned char *_buffer;
        unsigned int _capacity;
        unsigned int _offset;
    public:
        output_dynamic();

        explicit output_dynamic(unsigned int inital_capacity);

        ~output_dynamic();

        virtual unsigned char *data();

        virtual unsigned int size();

        virtual void put_byte(unsigned char value);

        virtual void put_bytes(const unsigned char *data, int size);

    private:
        void init(unsigned int initalCapacity);
    };
}



#endif //__CborDynamicOutput_H_
