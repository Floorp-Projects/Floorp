/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

tv = {
  raw: util.hex2abv("f3095c4fe5e299477643c2310b44f0aa"),

  // this key had an inappropriate length (18 octets)
  negative_raw: util.hex2abv("f3095c4fe5e299477643c2310b44f0aabbcc"),

  // openssl genrsa 512 | openssl pkcs8 -topk8 -nocrypt
  pkcs8: util.hex2abv(
    "30820154020100300d06092a864886f70d01010105000482013e3082013a0201" +
    "00024100a240ceb54e70dc14825b587d2f5dfd463c4b8250b696004a1acaafe4" +
    "9bcf384a46aa9fb4d9c7ee88e9ef0a315f53868f63680b58347249baedd93415" +
    "16c4cab70203010001024034e6dc7ed0ec8b55448b73f69d1310196e5f5045f0" +
    "c247a5e1c664432d6a0af7e7da40b83af047dd01f5e0a90e47c224d7b5133a35" +
    "4d11aa5003b3e8546c9901022100cdb2d7a7435bcb45e50e86f6c14e97ed781f" +
    "0956cd26e6f75ed9fc88125f8407022100c9ee30af6cb95ac9c1149ed84b3338" +
    "481741359409f369c497be177d950fb7d10221008b0ef98d611320639b0b6c20" +
    "4ae4a7fee8f30a6c3cfaacafd4d6c74af228d26702206b0e1dbf935bbd774327" +
    "2483b572a53f0b1d2643a2f6eab7305fb6627cf9855102203d2263156b324146" +
    "4478b713eb854c4f6b3ef052f0463b65d8217daec0099834"
  ),

  // Truncated form of the PKCS#8 stucture above
  negative_pkcs8: util.hex2abv("30820154020100300d06092a864886f70d010"),

  // Extracted from a cert via http://lapo.it/asn1js/
  spki: util.hex2abv(
    "30819F300D06092A864886F70D010101050003818D0030818902818100895309" +
    "7086EE6147C5F4D5FFAF1B498A3D11EC5518E964DC52126B2614F743883F64CA" +
    "51377ABB530DFD20464A48BD67CD27E7B29AEC685C5D10825E605C056E4AB8EE" +
    "A460FA27E55AA62C498B02D7247A249838A12ECDF37C6011CF4F0EDEA9CEE687" +
    "C1CB4A51C6AE62B2EFDB000723A01C99D6C23F834880BA8B42D5414E6F020301" +
    "0001"
  ),

  // Truncated form of the PKCS#8 stucture above
  negative_spki: util.hex2abv("30819F300D06092A864886F70D010101050003"),

  // From the NESSIE project
  // https://www.cosic.esat.kuleuven.be/nessie/testvectors/hash
  //        /sha/Sha-2-256.unverified.test-vectors
  // Set 1, vector# 5
  sha256: {
    data: util.hex2abv("616263646263646563646566646566676566676866676" +
                       "8696768696a68696a6b696a6b6c6a6b6c6d6b6c6d6e6c" +
                       "6d6e6f6d6e6f706e6f7071"),
    result: util.hex2abv("248D6A61D20638B8E5C026930C3E6039A33CE45964F" +
                         "F2167F6ECEDD419DB06C1"),
  },
}
