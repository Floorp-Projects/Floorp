cbor-cpp
========

[![Gitter](https://badges.gitter.im/Join%20Chat.svg)](https://gitter.im/naphaso/cbor-cpp?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)

CBOR C++ serialization library

Just a simple SAX-like Concise Binary Object Representation (CBOR).

[http://tools.ietf.org/html/rfc7049](http://tools.ietf.org/html/rfc7049)

#### Examples

```C++
    cbor::output_dynamic output;

    { //encoding
        cbor::encoder encoder(output);
        encoder.write_array(5);
        {
            encoder.write_int(123);
            encoder.write_string("bar");
            encoder.write_int(321);
            encoder.write_int(321);
            encoder.write_string("foo");
        }
    }

    { // decoding
        cbor::input input(output.data(), output.size());
        cbor::listener_debug listener;
        cbor::decoder decoder(input, listener);
        decoder.run();
    }
```
