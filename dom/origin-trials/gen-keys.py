# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import sys

from pyasn1.codec.der import decoder
from pyasn1.type import univ
from pyasn1_modules import pem


def public_key_to_string(file, name):
    out = "static const unsigned char " + name + "[65] = { "
    with open(file) as f:
        substrate = pem.readPemFromFile(
            f, "-----BEGIN PUBLIC KEY-----", "-----END PUBLIC KEY-----"
        )
        key = decoder.decode(substrate)
        ident = key[0][0]
        assert ident[0] == univ.ObjectIdentifier(
            "1.2.840.10045.2.1"
        ), "should be an ECPublicKey"
        assert ident[1] == univ.ObjectIdentifier(
            "1.2.840.10045.3.1.7"
        ), "should be a EcdsaP256 key"
        bits = key[0][1]
        assert isinstance(bits, univ.BitString), "Should be a bit string"
        assert len(bits) == 520, "Should be 520 bits (65 bytes)"
        for byte in bits.asOctets():
            out += hex(byte) + ", "
    out += "};"
    return out


def generate(output, test_key, prod_key):
    output.write(public_key_to_string(test_key, "kTestKey"))
    output.write("\n\n")
    output.write(public_key_to_string(prod_key, "kProdKey"))


if __name__ == "__main__":
    generate(sys.stdout, *sys.argv[1:])
