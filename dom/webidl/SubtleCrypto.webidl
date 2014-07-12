/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://www.w3.org/TR/WebCryptoAPI/
 */

typedef DOMString KeyType;
typedef DOMString KeyUsage;
typedef Uint8Array BigInteger;

/***** KeyAlgorithm interfaces *****/

[NoInterfaceObject]
interface KeyAlgorithm {
  readonly attribute DOMString name;
};

[NoInterfaceObject]
interface AesKeyAlgorithm : KeyAlgorithm {
  readonly attribute unsigned short length;
};

[NoInterfaceObject]
interface HmacKeyAlgorithm : KeyAlgorithm {
  readonly attribute KeyAlgorithm hash;
  readonly attribute unsigned long length;
};

[NoInterfaceObject]
interface RsaKeyAlgorithm : KeyAlgorithm {
  readonly attribute unsigned long modulusLength;
  [Throws]
  readonly attribute BigInteger publicExponent;
};

[NoInterfaceObject]
interface RsaHashedKeyAlgorithm : RsaKeyAlgorithm {
  readonly attribute KeyAlgorithm hash;
};


/***** Algorithm dictionaries *****/

dictionary Algorithm {
  DOMString name;
};

dictionary AesCbcParams : Algorithm {
  CryptoOperationData iv;
};

dictionary AesCtrParams : Algorithm {
  CryptoOperationData counter;
  [EnforceRange] octet length;
};

dictionary AesGcmParams : Algorithm {
  CryptoOperationData iv;
  CryptoOperationData additionalData;
  [EnforceRange] octet tagLength;
};

dictionary HmacImportParams : Algorithm {
  AlgorithmIdentifier hash;
};

dictionary Pbkdf2Params : Algorithm {
  CryptoOperationData salt;
  [EnforceRange] unsigned long iterations;
  AlgorithmIdentifier hash;
};

dictionary RsaHashedImportParams {
  AlgorithmIdentifier hash;
};

dictionary AesKeyGenParams : Algorithm {
  [EnforceRange] unsigned short length;
};

dictionary HmacKeyGenParams : Algorithm {
  AlgorithmIdentifier hash;
  [EnforceRange] unsigned long length;
};

dictionary RsaKeyGenParams : Algorithm {
  [EnforceRange] unsigned long modulusLength;
  BigInteger publicExponent;
};

dictionary RsaHashedKeyGenParams : RsaKeyGenParams {
  AlgorithmIdentifier hash;
};

dictionary DhKeyGenParams : Algorithm {
  BigInteger prime;
  BigInteger generator;
};

typedef DOMString NamedCurve;
dictionary EcKeyGenParams : Algorithm {
  NamedCurve namedCurve;
};

/***** The Main API *****/

[Pref="dom.webcrypto.enabled"]
interface CryptoKey {
  readonly attribute KeyType type;
  readonly attribute boolean extractable;
  readonly attribute KeyAlgorithm algorithm;
  [Cached, Constant, Frozen] readonly attribute sequence<KeyUsage> usages;
};

[Pref="dom.webcrypto.enabled"]
interface CryptoKeyPair {
  readonly attribute CryptoKey publicKey;
  readonly attribute CryptoKey privateKey;
};

typedef DOMString KeyFormat;
typedef (ArrayBufferView or ArrayBuffer) CryptoOperationData;
typedef (ArrayBufferView or ArrayBuffer) KeyData;
typedef (object or DOMString) AlgorithmIdentifier;

[Pref="dom.webcrypto.enabled"]
interface SubtleCrypto {
  Promise encrypt(AlgorithmIdentifier algorithm,
                  CryptoKey key,
                  CryptoOperationData data);
  Promise decrypt(AlgorithmIdentifier algorithm,
                  CryptoKey key,
                  CryptoOperationData data);
  Promise sign(AlgorithmIdentifier algorithm,
               CryptoKey key,
               CryptoOperationData data);
  Promise verify(AlgorithmIdentifier algorithm,
                 CryptoKey key,
                 CryptoOperationData signature,
                 CryptoOperationData data);
  Promise digest(AlgorithmIdentifier algorithm,
                 CryptoOperationData data);

  Promise generateKey(AlgorithmIdentifier algorithm,
                      boolean extractable,
                      sequence<KeyUsage> keyUsages );
  Promise deriveKey(AlgorithmIdentifier algorithm,
                    CryptoKey baseKey,
                    AlgorithmIdentifier derivedKeyType,
                    boolean extractable,
                    sequence<KeyUsage> keyUsages );
  Promise deriveBits(AlgorithmIdentifier algorithm,
                     CryptoKey baseKey,
                     unsigned long length);

  Promise importKey(KeyFormat format,
                    KeyData keyData,
                    AlgorithmIdentifier algorithm,
                    boolean extractable,
                    sequence<KeyUsage> keyUsages );
  Promise exportKey(KeyFormat format, CryptoKey key);
};

