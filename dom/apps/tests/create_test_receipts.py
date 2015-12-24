#!/usr/bin/env python

import jwt

receipt1 = {
  "typ": "purchase-receipt",
  "product": {
    "url": "https://www.mozilla.org",
    "storedata": "5169314356"
  },
  "user": {
    "type": "directed-identifier",
    "value": "4fb35151-2b9b-4ba2-8283-c49d381640bd"
  },
  "iss": "http://mochi.test:8888",
  "nbf": 131360185,
  "iat": 131360188,
  "detail": "http://mochi.test:8888/receipt/5169314356",
  "verify": "http://mochi.test:8888/verify/5169314356",
  "reissue": "http://mochi.test:8888/reissue/5169314356"
}

receipt2 = {
  "typ": "purchase-receipt",
  "product": {
    "url": "https://www.mozilla.org",
    "storedata": "5169314357"
  },
  "user": {
    "type": "directed-identifier",
    "value": "4fb35151-2b9b-4ba2-8283-c49d381640bd"
  },
  "iss": "http://mochi.test:8888",
  "nbf": 131360185,
  "iat": 131360188,
  "detail": "http://mochi.test:8888/receipt/5169314356",
  "verify": "http://mochi.test:8888/verify/5169314356",
  "reissue": "http://mochi.test:8888/reissue/5169314356"
}

receipt_without_typ = {
  "product": {
    "url": "https://www.mozilla.org",
    "storedata": "5169314358"
  },
  "user": {
    "type": "directed-identifier",
    "value": "4fb35151-2b9b-4ba2-8283-c49d381640bd"
  },
  "iss": "http://mochi.test:8888",
  "nbf": 131360185,
  "iat": 131360188,
  "detail": "http://mochi.test:8888/receipt/5169314356",
  "verify": "http://mochi.test:8888/verify/5169314356",
  "reissue": "http://mochi.test:8888/reissue/5169314356"
}

receipt_without_product = {
  "typ": "purchase-receipt",
  "user": {
    "type": "directed-identifier",
    "value": "4fb35151-2b9b-4ba2-8283-c49d381640bd"
  },
  "iss": "http://mochi.test:8888",
  "nbf": 131360185,
  "iat": 131360188,
  "detail": "http://mochi.test:8888/receipt/5169314356",
  "verify": "http://mochi.test:8888/verify/5169314356",
  "reissue": "http://mochi.test:8888/reissue/5169314356"
}

receipt_without_user = {
  "typ": "purchase-receipt",
  "product": {
    "url": "https://www.mozilla.org",
    "storedata": "5169314358"
  },
  "iss": "http://mochi.test:8888",
  "nbf": 131360185,
  "iat": 131360188,
  "detail": "http://mochi.test:8888/receipt/5169314356",
  "verify": "http://mochi.test:8888/verify/5169314356",
  "reissue": "http://mochi.test:8888/reissue/5169314356"
}

receipt_without_iss = {
  "typ": "purchase-receipt",
  "product": {
    "url": "https://www.mozilla.org",
    "storedata": "5169314358"
  },
  "user": {
    "type": "directed-identifier",
    "value": "4fb35151-2b9b-4ba2-8283-c49d381640bd"
  },
  "nbf": 131360185,
  "iat": 131360188,
  "detail": "http://mochi.test:8888/receipt/5169314356",
  "verify": "http://mochi.test:8888/verify/5169314356",
  "reissue": "http://mochi.test:8888/reissue/5169314356"
}

receipt_without_nbf = {
  "typ": "purchase-receipt",
  "product": {
    "url": "https://www.mozilla.org",
    "storedata": "5169314358"
  },
  "user": {
    "type": "directed-identifier",
    "value": "4fb35151-2b9b-4ba2-8283-c49d381640bd"
  },
  "iss": "http://mochi.test:8888",
  "iat": 131360188,
  "detail": "http://mochi.test:8888/receipt/5169314356",
  "verify": "http://mochi.test:8888/verify/5169314356",
  "reissue": "http://mochi.test:8888/reissue/5169314356"
}

receipt_without_iat = {
  "typ": "purchase-receipt",
  "product": {
    "url": "https://www.mozilla.org",
    "storedata": "5169314358"
  },
  "user": {
    "type": "directed-identifier",
    "value": "4fb35151-2b9b-4ba2-8283-c49d381640bd"
  },
  "iss": "http://mochi.test:8888",
  "nbf": 131360185,
  "detail": "http://mochi.test:8888/receipt/5169314356",
  "verify": "http://mochi.test:8888/verify/5169314356",
  "reissue": "http://mochi.test:8888/reissue/5169314356"
}

receipt_with_wrong_typ = {
  "typ": "fake",
  "product": {
    "url": "https://www.mozilla.org",
    "storedata": "5169314358"
  },
  "user": {
    "type": "directed-identifier",
    "value": "4fb35151-2b9b-4ba2-8283-c49d381640bd"
  },
  "iss": "http://mochi.test:8888",
  "nbf": 131360185,
  "iat": 131360188,
  "detail": "http://mochi.test:8888/receipt/5169314356",
  "verify": "http://mochi.test:8888/verify/5169314356",
  "reissue": "http://mochi.test:8888/reissue/5169314356"
}

print("let valid_receipt1 = \"" + jwt.encode(receipt1, "") + "\";\n")
print("let valid_receipt2 = \"" + jwt.encode(receipt2, "") + "\";\n")
print("let receipt_without_typ = \"" + jwt.encode(receipt_without_typ, "") + "\";\n")
print("let receipt_without_product = \"" + jwt.encode(receipt_without_product, "") + "\";\n")
print("let receipt_without_user = \"" + jwt.encode(receipt_without_user, "") + "\";\n")
print("let receipt_without_iss = \"" + jwt.encode(receipt_without_iss, "") + "\";\n")
print("let receipt_without_nbf = \"" + jwt.encode(receipt_without_nbf, "") + "\";\n")
print("let receipt_without_iat = \"" + jwt.encode(receipt_without_iat, "") + "\";\n")
print("let receipt_with_wrong_typ = \"" + jwt.encode(receipt_with_wrong_typ, "") + "\";\n")
