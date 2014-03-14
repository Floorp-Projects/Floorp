// Copyright 2013 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// generated_language.cc
// Machine generated. Do Not Edit.
//
// Declarations for languages recognized by CLD2
//

#include "generated_language.h"
#include "generated_ulscript.h"

namespace CLD2 {

// Subscripted by enum Language
extern const int kLanguageToNameSize = 614;
extern const char* const kLanguageToName[kLanguageToNameSize] = {
  "ENGLISH",               // 0 en
  "DANISH",                // 1 da
  "DUTCH",                 // 2 nl
  "FINNISH",               // 3 fi
  "FRENCH",                // 4 fr
  "GERMAN",                // 5 de
  "HEBREW",                // 6 iw
  "ITALIAN",               // 7 it
  "Japanese",              // 8 ja
  "Korean",                // 9 ko
  "NORWEGIAN",             // 10 no
  "POLISH",                // 11 pl
  "PORTUGUESE",            // 12 pt
  "RUSSIAN",               // 13 ru
  "SPANISH",               // 14 es
  "SWEDISH",               // 15 sv
  "Chinese",               // 16 zh
  "CZECH",                 // 17 cs
  "GREEK",                 // 18 el
  "ICELANDIC",             // 19 is
  "LATVIAN",               // 20 lv
  "LITHUANIAN",            // 21 lt
  "ROMANIAN",              // 22 ro
  "HUNGARIAN",             // 23 hu
  "ESTONIAN",              // 24 et
  "Ignore",                // 25 xxx
  "Unknown",               // 26 un
  "BULGARIAN",             // 27 bg
  "CROATIAN",              // 28 hr
  "SERBIAN",               // 29 sr
  "IRISH",                 // 30 ga
  "GALICIAN",              // 31 gl
  "TAGALOG",               // 32 tl
  "TURKISH",               // 33 tr
  "UKRAINIAN",             // 34 uk
  "HINDI",                 // 35 hi
  "MACEDONIAN",            // 36 mk
  "BENGALI",               // 37 bn
  "INDONESIAN",            // 38 id
  "LATIN",                 // 39 la
  "MALAY",                 // 40 ms
  "MALAYALAM",             // 41 ml
  "WELSH",                 // 42 cy
  "NEPALI",                // 43 ne
  "TELUGU",                // 44 te
  "ALBANIAN",              // 45 sq
  "TAMIL",                 // 46 ta
  "BELARUSIAN",            // 47 be
  "JAVANESE",              // 48 jw
  "OCCITAN",               // 49 oc
  "URDU",                  // 50 ur
  "BIHARI",                // 51 bh
  "GUJARATI",              // 52 gu
  "THAI",                  // 53 th
  "ARABIC",                // 54 ar
  "CATALAN",               // 55 ca
  "ESPERANTO",             // 56 eo
  "BASQUE",                // 57 eu
  "INTERLINGUA",           // 58 ia
  "KANNADA",               // 59 kn
  "PUNJABI",               // 60 pa
  "SCOTS_GAELIC",          // 61 gd
  "SWAHILI",               // 62 sw
  "SLOVENIAN",             // 63 sl
  "MARATHI",               // 64 mr
  "MALTESE",               // 65 mt
  "VIETNAMESE",            // 66 vi
  "FRISIAN",               // 67 fy
  "SLOVAK",                // 68 sk
  "ChineseT",              // 69 zh-Hant
  "FAROESE",               // 70 fo
  "SUNDANESE",             // 71 su
  "UZBEK",                 // 72 uz
  "AMHARIC",               // 73 am
  "AZERBAIJANI",           // 74 az
  "GEORGIAN",              // 75 ka
  "TIGRINYA",              // 76 ti
  "PERSIAN",               // 77 fa
  "BOSNIAN",               // 78 bs
  "SINHALESE",             // 79 si
  "NORWEGIAN_N",           // 80 nn
  "81",                    // 81
  "82",                    // 82
  "XHOSA",                 // 83 xh
  "ZULU",                  // 84 zu
  "GUARANI",               // 85 gn
  "SESOTHO",               // 86 st
  "TURKMEN",               // 87 tk
  "KYRGYZ",                // 88 ky
  "BRETON",                // 89 br
  "TWI",                   // 90 tw
  "YIDDISH",               // 91 yi
  "92",                    // 92
  "SOMALI",                // 93 so
  "UIGHUR",                // 94 ug
  "KURDISH",               // 95 ku
  "MONGOLIAN",             // 96 mn
  "ARMENIAN",              // 97 hy
  "LAOTHIAN",              // 98 lo
  "SINDHI",                // 99 sd
  "RHAETO_ROMANCE",        // 100 rm
  "AFRIKAANS",             // 101 af
  "LUXEMBOURGISH",         // 102 lb
  "BURMESE",               // 103 my
  "KHMER",                 // 104 km
  "TIBETAN",               // 105 bo
  "DHIVEHI",               // 106 dv
  "CHEROKEE",              // 107 chr
  "SYRIAC",                // 108 syr
  "LIMBU",                 // 109 lif
  "ORIYA",                 // 110 or
  "ASSAMESE",              // 111 as
  "CORSICAN",              // 112 co
  "INTERLINGUE",           // 113 ie
  "KAZAKH",                // 114 kk
  "LINGALA",               // 115 ln
  "116",                   // 116
  "PASHTO",                // 117 ps
  "QUECHUA",               // 118 qu
  "SHONA",                 // 119 sn
  "TAJIK",                 // 120 tg
  "TATAR",                 // 121 tt
  "TONGA",                 // 122 to
  "YORUBA",                // 123 yo
  "124",                   // 124
  "125",                   // 125
  "126",                   // 126
  "127",                   // 127
  "MAORI",                 // 128 mi
  "WOLOF",                 // 129 wo
  "ABKHAZIAN",             // 130 ab
  "AFAR",                  // 131 aa
  "AYMARA",                // 132 ay
  "BASHKIR",               // 133 ba
  "BISLAMA",               // 134 bi
  "DZONGKHA",              // 135 dz
  "FIJIAN",                // 136 fj
  "GREENLANDIC",           // 137 kl
  "HAUSA",                 // 138 ha
  "HAITIAN_CREOLE",        // 139 ht
  "INUPIAK",               // 140 ik
  "INUKTITUT",             // 141 iu
  "KASHMIRI",              // 142 ks
  "KINYARWANDA",           // 143 rw
  "MALAGASY",              // 144 mg
  "NAURU",                 // 145 na
  "OROMO",                 // 146 om
  "RUNDI",                 // 147 rn
  "SAMOAN",                // 148 sm
  "SANGO",                 // 149 sg
  "SANSKRIT",              // 150 sa
  "SISWANT",               // 151 ss
  "TSONGA",                // 152 ts
  "TSWANA",                // 153 tn
  "VOLAPUK",               // 154 vo
  "ZHUANG",                // 155 za
  "KHASI",                 // 156 kha
  "SCOTS",                 // 157 sco
  "GANDA",                 // 158 lg
  "MANX",                  // 159 gv
  "MONTENEGRIN",           // 160 sr-ME
  "AKAN",                  // 161 ak
  "IGBO",                  // 162 ig
  "MAURITIAN_CREOLE",      // 163 mfe
  "HAWAIIAN",              // 164 haw
  "CEBUANO",               // 165 ceb
  "EWE",                   // 166 ee
  "GA",                    // 167 gaa
  "HMONG",                 // 168 hmn
  "KRIO",                  // 169 kri
  "LOZI",                  // 170 loz
  "LUBA_LULUA",            // 171 lua
  "LUO_KENYA_AND_TANZANIA",  // 172 luo
  "NEWARI",                // 173 new
  "NYANJA",                // 174 ny
  "OSSETIAN",              // 175 os
  "PAMPANGA",              // 176 pam
  "PEDI",                  // 177 nso
  "RAJASTHANI",            // 178 raj
  "SESELWA",               // 179 crs
  "TUMBUKA",               // 180 tum
  "VENDA",                 // 181 ve
  "WARAY_PHILIPPINES",     // 182 war
  "183",                   // 183
  "184",                   // 184
  "185",                   // 185
  "186",                   // 186
  "187",                   // 187
  "188",                   // 188
  "189",                   // 189
  "190",                   // 190
  "191",                   // 191
  "192",                   // 192
  "193",                   // 193
  "194",                   // 194
  "195",                   // 195
  "196",                   // 196
  "197",                   // 197
  "198",                   // 198
  "199",                   // 199
  "200",                   // 200
  "201",                   // 201
  "202",                   // 202
  "203",                   // 203
  "204",                   // 204
  "205",                   // 205
  "206",                   // 206
  "207",                   // 207
  "208",                   // 208
  "209",                   // 209
  "210",                   // 210
  "211",                   // 211
  "212",                   // 212
  "213",                   // 213
  "214",                   // 214
  "215",                   // 215
  "216",                   // 216
  "217",                   // 217
  "218",                   // 218
  "219",                   // 219
  "220",                   // 220
  "221",                   // 221
  "222",                   // 222
  "223",                   // 223
  "224",                   // 224
  "225",                   // 225
  "226",                   // 226
  "227",                   // 227
  "228",                   // 228
  "229",                   // 229
  "230",                   // 230
  "231",                   // 231
  "232",                   // 232
  "233",                   // 233
  "234",                   // 234
  "235",                   // 235
  "236",                   // 236
  "237",                   // 237
  "238",                   // 238
  "239",                   // 239
  "240",                   // 240
  "241",                   // 241
  "242",                   // 242
  "243",                   // 243
  "244",                   // 244
  "245",                   // 245
  "246",                   // 246
  "247",                   // 247
  "248",                   // 248
  "249",                   // 249
  "250",                   // 250
  "251",                   // 251
  "252",                   // 252
  "253",                   // 253
  "254",                   // 254
  "255",                   // 255
  "256",                   // 256
  "257",                   // 257
  "258",                   // 258
  "259",                   // 259
  "260",                   // 260
  "261",                   // 261
  "262",                   // 262
  "263",                   // 263
  "264",                   // 264
  "265",                   // 265
  "266",                   // 266
  "267",                   // 267
  "268",                   // 268
  "269",                   // 269
  "270",                   // 270
  "271",                   // 271
  "272",                   // 272
  "273",                   // 273
  "274",                   // 274
  "275",                   // 275
  "276",                   // 276
  "277",                   // 277
  "278",                   // 278
  "279",                   // 279
  "280",                   // 280
  "281",                   // 281
  "282",                   // 282
  "283",                   // 283
  "284",                   // 284
  "285",                   // 285
  "286",                   // 286
  "287",                   // 287
  "288",                   // 288
  "289",                   // 289
  "290",                   // 290
  "291",                   // 291
  "292",                   // 292
  "293",                   // 293
  "294",                   // 294
  "295",                   // 295
  "296",                   // 296
  "297",                   // 297
  "298",                   // 298
  "299",                   // 299
  "300",                   // 300
  "301",                   // 301
  "302",                   // 302
  "303",                   // 303
  "304",                   // 304
  "305",                   // 305
  "306",                   // 306
  "307",                   // 307
  "308",                   // 308
  "309",                   // 309
  "310",                   // 310
  "311",                   // 311
  "312",                   // 312
  "313",                   // 313
  "314",                   // 314
  "315",                   // 315
  "316",                   // 316
  "317",                   // 317
  "318",                   // 318
  "319",                   // 319
  "320",                   // 320
  "321",                   // 321
  "322",                   // 322
  "323",                   // 323
  "324",                   // 324
  "325",                   // 325
  "326",                   // 326
  "327",                   // 327
  "328",                   // 328
  "329",                   // 329
  "330",                   // 330
  "331",                   // 331
  "332",                   // 332
  "333",                   // 333
  "334",                   // 334
  "335",                   // 335
  "336",                   // 336
  "337",                   // 337
  "338",                   // 338
  "339",                   // 339
  "340",                   // 340
  "341",                   // 341
  "342",                   // 342
  "343",                   // 343
  "344",                   // 344
  "345",                   // 345
  "346",                   // 346
  "347",                   // 347
  "348",                   // 348
  "349",                   // 349
  "350",                   // 350
  "351",                   // 351
  "352",                   // 352
  "353",                   // 353
  "354",                   // 354
  "355",                   // 355
  "356",                   // 356
  "357",                   // 357
  "358",                   // 358
  "359",                   // 359
  "360",                   // 360
  "361",                   // 361
  "362",                   // 362
  "363",                   // 363
  "364",                   // 364
  "365",                   // 365
  "366",                   // 366
  "367",                   // 367
  "368",                   // 368
  "369",                   // 369
  "370",                   // 370
  "371",                   // 371
  "372",                   // 372
  "373",                   // 373
  "374",                   // 374
  "375",                   // 375
  "376",                   // 376
  "377",                   // 377
  "378",                   // 378
  "379",                   // 379
  "380",                   // 380
  "381",                   // 381
  "382",                   // 382
  "383",                   // 383
  "384",                   // 384
  "385",                   // 385
  "386",                   // 386
  "387",                   // 387
  "388",                   // 388
  "389",                   // 389
  "390",                   // 390
  "391",                   // 391
  "392",                   // 392
  "393",                   // 393
  "394",                   // 394
  "395",                   // 395
  "396",                   // 396
  "397",                   // 397
  "398",                   // 398
  "399",                   // 399
  "400",                   // 400
  "401",                   // 401
  "402",                   // 402
  "403",                   // 403
  "404",                   // 404
  "405",                   // 405
  "406",                   // 406
  "407",                   // 407
  "408",                   // 408
  "409",                   // 409
  "410",                   // 410
  "411",                   // 411
  "412",                   // 412
  "413",                   // 413
  "414",                   // 414
  "415",                   // 415
  "416",                   // 416
  "417",                   // 417
  "418",                   // 418
  "419",                   // 419
  "420",                   // 420
  "421",                   // 421
  "422",                   // 422
  "423",                   // 423
  "424",                   // 424
  "425",                   // 425
  "426",                   // 426
  "427",                   // 427
  "428",                   // 428
  "429",                   // 429
  "430",                   // 430
  "431",                   // 431
  "432",                   // 432
  "433",                   // 433
  "434",                   // 434
  "435",                   // 435
  "436",                   // 436
  "437",                   // 437
  "438",                   // 438
  "439",                   // 439
  "440",                   // 440
  "441",                   // 441
  "442",                   // 442
  "443",                   // 443
  "444",                   // 444
  "445",                   // 445
  "446",                   // 446
  "447",                   // 447
  "448",                   // 448
  "449",                   // 449
  "450",                   // 450
  "451",                   // 451
  "452",                   // 452
  "453",                   // 453
  "454",                   // 454
  "455",                   // 455
  "456",                   // 456
  "457",                   // 457
  "458",                   // 458
  "459",                   // 459
  "460",                   // 460
  "461",                   // 461
  "462",                   // 462
  "463",                   // 463
  "464",                   // 464
  "465",                   // 465
  "466",                   // 466
  "467",                   // 467
  "468",                   // 468
  "469",                   // 469
  "470",                   // 470
  "471",                   // 471
  "472",                   // 472
  "473",                   // 473
  "474",                   // 474
  "475",                   // 475
  "476",                   // 476
  "477",                   // 477
  "478",                   // 478
  "479",                   // 479
  "480",                   // 480
  "481",                   // 481
  "482",                   // 482
  "483",                   // 483
  "484",                   // 484
  "485",                   // 485
  "486",                   // 486
  "487",                   // 487
  "488",                   // 488
  "489",                   // 489
  "490",                   // 490
  "491",                   // 491
  "492",                   // 492
  "493",                   // 493
  "494",                   // 494
  "495",                   // 495
  "496",                   // 496
  "497",                   // 497
  "498",                   // 498
  "499",                   // 499
  "500",                   // 500
  "501",                   // 501
  "502",                   // 502
  "503",                   // 503
  "504",                   // 504
  "505",                   // 505
  "NDEBELE",               // 506 nr
  "X_BORK_BORK_BORK",      // 507 zzb
  "X_PIG_LATIN",           // 508 zzp
  "X_HACKER",              // 509 zzh
  "X_KLINGON",             // 510 tlh
  "X_ELMER_FUDD",          // 511 zze
  "X_Common",              // 512 xx-Zyyy
  "X_Latin",               // 513 xx-Latn
  "X_Greek",               // 514 xx-Grek
  "X_Cyrillic",            // 515 xx-Cyrl
  "X_Armenian",            // 516 xx-Armn
  "X_Hebrew",              // 517 xx-Hebr
  "X_Arabic",              // 518 xx-Arab
  "X_Syriac",              // 519 xx-Syrc
  "X_Thaana",              // 520 xx-Thaa
  "X_Devanagari",          // 521 xx-Deva
  "X_Bengali",             // 522 xx-Beng
  "X_Gurmukhi",            // 523 xx-Guru
  "X_Gujarati",            // 524 xx-Gujr
  "X_Oriya",               // 525 xx-Orya
  "X_Tamil",               // 526 xx-Taml
  "X_Telugu",              // 527 xx-Telu
  "X_Kannada",             // 528 xx-Knda
  "X_Malayalam",           // 529 xx-Mlym
  "X_Sinhala",             // 530 xx-Sinh
  "X_Thai",                // 531 xx-Thai
  "X_Lao",                 // 532 xx-Laoo
  "X_Tibetan",             // 533 xx-Tibt
  "X_Myanmar",             // 534 xx-Mymr
  "X_Georgian",            // 535 xx-Geor
  "X_Hangul",              // 536 xx-Hang
  "X_Ethiopic",            // 537 xx-Ethi
  "X_Cherokee",            // 538 xx-Cher
  "X_Canadian_Aboriginal",  // 539 xx-Cans
  "X_Ogham",               // 540 xx-Ogam
  "X_Runic",               // 541 xx-Runr
  "X_Khmer",               // 542 xx-Khmr
  "X_Mongolian",           // 543 xx-Mong
  "X_Hiragana",            // 544 xx-Hira
  "X_Katakana",            // 545 xx-Kana
  "X_Bopomofo",            // 546 xx-Bopo
  "X_Han",                 // 547 xx-Hani
  "X_Yi",                  // 548 xx-Yiii
  "X_Old_Italic",          // 549 xx-Ital
  "X_Gothic",              // 550 xx-Goth
  "X_Deseret",             // 551 xx-Dsrt
  "X_Inherited",           // 552 xx-Qaai
  "X_Tagalog",             // 553 xx-Tglg
  "X_Hanunoo",             // 554 xx-Hano
  "X_Buhid",               // 555 xx-Buhd
  "X_Tagbanwa",            // 556 xx-Tagb
  "X_Limbu",               // 557 xx-Limb
  "X_Tai_Le",              // 558 xx-Tale
  "X_Linear_B",            // 559 xx-Linb
  "X_Ugaritic",            // 560 xx-Ugar
  "X_Shavian",             // 561 xx-Shaw
  "X_Osmanya",             // 562 xx-Osma
  "X_Cypriot",             // 563 xx-Cprt
  "X_Braille",             // 564 xx-Brai
  "X_Buginese",            // 565 xx-Bugi
  "X_Coptic",              // 566 xx-Copt
  "X_New_Tai_Lue",         // 567 xx-Talu
  "X_Glagolitic",          // 568 xx-Glag
  "X_Tifinagh",            // 569 xx-Tfng
  "X_Syloti_Nagri",        // 570 xx-Sylo
  "X_Old_Persian",         // 571 xx-Xpeo
  "X_Kharoshthi",          // 572 xx-Khar
  "X_Balinese",            // 573 xx-Bali
  "X_Cuneiform",           // 574 xx-Xsux
  "X_Phoenician",          // 575 xx-Phnx
  "X_Phags_Pa",            // 576 xx-Phag
  "X_Nko",                 // 577 xx-Nkoo
  "X_Sundanese",           // 578 xx-Sund
  "X_Lepcha",              // 579 xx-Lepc
  "X_Ol_Chiki",            // 580 xx-Olck
  "X_Vai",                 // 581 xx-Vaii
  "X_Saurashtra",          // 582 xx-Saur
  "X_Kayah_Li",            // 583 xx-Kali
  "X_Rejang",              // 584 xx-Rjng
  "X_Lycian",              // 585 xx-Lyci
  "X_Carian",              // 586 xx-Cari
  "X_Lydian",              // 587 xx-Lydi
  "X_Cham",                // 588 xx-Cham
  "X_Tai_Tham",            // 589 xx-Lana
  "X_Tai_Viet",            // 590 xx-Tavt
  "X_Avestan",             // 591 xx-Avst
  "X_Egyptian_Hieroglyphs",  // 592 xx-Egyp
  "X_Samaritan",           // 593 xx-Samr
  "X_Lisu",                // 594 xx-Lisu
  "X_Bamum",               // 595 xx-Bamu
  "X_Javanese",            // 596 xx-Java
  "X_Meetei_Mayek",        // 597 xx-Mtei
  "X_Imperial_Aramaic",    // 598 xx-Armi
  "X_Old_South_Arabian",   // 599 xx-Sarb
  "X_Inscriptional_Parthian",  // 600 xx-Prti
  "X_Inscriptional_Pahlavi",  // 601 xx-Phli
  "X_Old_Turkic",          // 602 xx-Orkh
  "X_Kaithi",              // 603 xx-Kthi
  "X_Batak",               // 604 xx-Batk
  "X_Brahmi",              // 605 xx-Brah
  "X_Mandaic",             // 606 xx-Mand
  "X_Chakma",              // 607 xx-Cakm
  "X_Meroitic_Cursive",    // 608 xx-Merc
  "X_Meroitic_Hieroglyphs",  // 609 xx-Mero
  "X_Miao",                // 610 xx-Plrd
  "X_Sharada",             // 611 xx-Shrd
  "X_Sora_Sompeng",        // 612 xx-Sora
  "X_Takri",               // 613 xx-Takr
};

// Subscripted by enum Language
extern const int kLanguageToCodeSize = 614;
extern const char* const kLanguageToCode[kLanguageToCodeSize] = {
  "en",    // 0 ENGLISH
  "da",    // 1 DANISH
  "nl",    // 2 DUTCH
  "fi",    // 3 FINNISH
  "fr",    // 4 FRENCH
  "de",    // 5 GERMAN
  "iw",    // 6 HEBREW
  "it",    // 7 ITALIAN
  "ja",    // 8 Japanese
  "ko",    // 9 Korean
  "no",    // 10 NORWEGIAN
  "pl",    // 11 POLISH
  "pt",    // 12 PORTUGUESE
  "ru",    // 13 RUSSIAN
  "es",    // 14 SPANISH
  "sv",    // 15 SWEDISH
  "zh",    // 16 Chinese
  "cs",    // 17 CZECH
  "el",    // 18 GREEK
  "is",    // 19 ICELANDIC
  "lv",    // 20 LATVIAN
  "lt",    // 21 LITHUANIAN
  "ro",    // 22 ROMANIAN
  "hu",    // 23 HUNGARIAN
  "et",    // 24 ESTONIAN
  "xxx",   // 25 Ignore
  "un",    // 26 Unknown
  "bg",    // 27 BULGARIAN
  "hr",    // 28 CROATIAN
  "sr",    // 29 SERBIAN
  "ga",    // 30 IRISH
  "gl",    // 31 GALICIAN
  "tl",    // 32 TAGALOG
  "tr",    // 33 TURKISH
  "uk",    // 34 UKRAINIAN
  "hi",    // 35 HINDI
  "mk",    // 36 MACEDONIAN
  "bn",    // 37 BENGALI
  "id",    // 38 INDONESIAN
  "la",    // 39 LATIN
  "ms",    // 40 MALAY
  "ml",    // 41 MALAYALAM
  "cy",    // 42 WELSH
  "ne",    // 43 NEPALI
  "te",    // 44 TELUGU
  "sq",    // 45 ALBANIAN
  "ta",    // 46 TAMIL
  "be",    // 47 BELARUSIAN
  "jw",    // 48 JAVANESE
  "oc",    // 49 OCCITAN
  "ur",    // 50 URDU
  "bh",    // 51 BIHARI
  "gu",    // 52 GUJARATI
  "th",    // 53 THAI
  "ar",    // 54 ARABIC
  "ca",    // 55 CATALAN
  "eo",    // 56 ESPERANTO
  "eu",    // 57 BASQUE
  "ia",    // 58 INTERLINGUA
  "kn",    // 59 KANNADA
  "pa",    // 60 PUNJABI
  "gd",    // 61 SCOTS_GAELIC
  "sw",    // 62 SWAHILI
  "sl",    // 63 SLOVENIAN
  "mr",    // 64 MARATHI
  "mt",    // 65 MALTESE
  "vi",    // 66 VIETNAMESE
  "fy",    // 67 FRISIAN
  "sk",    // 68 SLOVAK
  "zh-Hant",  // 69 ChineseT
  "fo",    // 70 FAROESE
  "su",    // 71 SUNDANESE
  "uz",    // 72 UZBEK
  "am",    // 73 AMHARIC
  "az",    // 74 AZERBAIJANI
  "ka",    // 75 GEORGIAN
  "ti",    // 76 TIGRINYA
  "fa",    // 77 PERSIAN
  "bs",    // 78 BOSNIAN
  "si",    // 79 SINHALESE
  "nn",    // 80 NORWEGIAN_N
  "",      // 81 81
  "",      // 82 82
  "xh",    // 83 XHOSA
  "zu",    // 84 ZULU
  "gn",    // 85 GUARANI
  "st",    // 86 SESOTHO
  "tk",    // 87 TURKMEN
  "ky",    // 88 KYRGYZ
  "br",    // 89 BRETON
  "tw",    // 90 TWI
  "yi",    // 91 YIDDISH
  "",      // 92 92
  "so",    // 93 SOMALI
  "ug",    // 94 UIGHUR
  "ku",    // 95 KURDISH
  "mn",    // 96 MONGOLIAN
  "hy",    // 97 ARMENIAN
  "lo",    // 98 LAOTHIAN
  "sd",    // 99 SINDHI
  "rm",    // 100 RHAETO_ROMANCE
  "af",    // 101 AFRIKAANS
  "lb",    // 102 LUXEMBOURGISH
  "my",    // 103 BURMESE
  "km",    // 104 KHMER
  "bo",    // 105 TIBETAN
  "dv",    // 106 DHIVEHI
  "chr",   // 107 CHEROKEE
  "syr",   // 108 SYRIAC
  "lif",   // 109 LIMBU
  "or",    // 110 ORIYA
  "as",    // 111 ASSAMESE
  "co",    // 112 CORSICAN
  "ie",    // 113 INTERLINGUE
  "kk",    // 114 KAZAKH
  "ln",    // 115 LINGALA
  "",      // 116 116
  "ps",    // 117 PASHTO
  "qu",    // 118 QUECHUA
  "sn",    // 119 SHONA
  "tg",    // 120 TAJIK
  "tt",    // 121 TATAR
  "to",    // 122 TONGA
  "yo",    // 123 YORUBA
  "",      // 124 124
  "",      // 125 125
  "",      // 126 126
  "",      // 127 127
  "mi",    // 128 MAORI
  "wo",    // 129 WOLOF
  "ab",    // 130 ABKHAZIAN
  "aa",    // 131 AFAR
  "ay",    // 132 AYMARA
  "ba",    // 133 BASHKIR
  "bi",    // 134 BISLAMA
  "dz",    // 135 DZONGKHA
  "fj",    // 136 FIJIAN
  "kl",    // 137 GREENLANDIC
  "ha",    // 138 HAUSA
  "ht",    // 139 HAITIAN_CREOLE
  "ik",    // 140 INUPIAK
  "iu",    // 141 INUKTITUT
  "ks",    // 142 KASHMIRI
  "rw",    // 143 KINYARWANDA
  "mg",    // 144 MALAGASY
  "na",    // 145 NAURU
  "om",    // 146 OROMO
  "rn",    // 147 RUNDI
  "sm",    // 148 SAMOAN
  "sg",    // 149 SANGO
  "sa",    // 150 SANSKRIT
  "ss",    // 151 SISWANT
  "ts",    // 152 TSONGA
  "tn",    // 153 TSWANA
  "vo",    // 154 VOLAPUK
  "za",    // 155 ZHUANG
  "kha",   // 156 KHASI
  "sco",   // 157 SCOTS
  "lg",    // 158 GANDA
  "gv",    // 159 MANX
  "sr-ME",  // 160 MONTENEGRIN
  "ak",    // 161 AKAN
  "ig",    // 162 IGBO
  "mfe",   // 163 MAURITIAN_CREOLE
  "haw",   // 164 HAWAIIAN
  "ceb",   // 165 CEBUANO
  "ee",    // 166 EWE
  "gaa",   // 167 GA
  "hmn",   // 168 HMONG
  "kri",   // 169 KRIO
  "loz",   // 170 LOZI
  "lua",   // 171 LUBA_LULUA
  "luo",   // 172 LUO_KENYA_AND_TANZANIA
  "new",   // 173 NEWARI
  "ny",    // 174 NYANJA
  "os",    // 175 OSSETIAN
  "pam",   // 176 PAMPANGA
  "nso",   // 177 PEDI
  "raj",   // 178 RAJASTHANI
  "crs",   // 179 SESELWA
  "tum",   // 180 TUMBUKA
  "ve",    // 181 VENDA
  "war",   // 182 WARAY_PHILIPPINES
  "",      // 183 183
  "",      // 184 184
  "",      // 185 185
  "",      // 186 186
  "",      // 187 187
  "",      // 188 188
  "",      // 189 189
  "",      // 190 190
  "",      // 191 191
  "",      // 192 192
  "",      // 193 193
  "",      // 194 194
  "",      // 195 195
  "",      // 196 196
  "",      // 197 197
  "",      // 198 198
  "",      // 199 199
  "",      // 200 200
  "",      // 201 201
  "",      // 202 202
  "",      // 203 203
  "",      // 204 204
  "",      // 205 205
  "",      // 206 206
  "",      // 207 207
  "",      // 208 208
  "",      // 209 209
  "",      // 210 210
  "",      // 211 211
  "",      // 212 212
  "",      // 213 213
  "",      // 214 214
  "",      // 215 215
  "",      // 216 216
  "",      // 217 217
  "",      // 218 218
  "",      // 219 219
  "",      // 220 220
  "",      // 221 221
  "",      // 222 222
  "",      // 223 223
  "",      // 224 224
  "",      // 225 225
  "",      // 226 226
  "",      // 227 227
  "",      // 228 228
  "",      // 229 229
  "",      // 230 230
  "",      // 231 231
  "",      // 232 232
  "",      // 233 233
  "",      // 234 234
  "",      // 235 235
  "",      // 236 236
  "",      // 237 237
  "",      // 238 238
  "",      // 239 239
  "",      // 240 240
  "",      // 241 241
  "",      // 242 242
  "",      // 243 243
  "",      // 244 244
  "",      // 245 245
  "",      // 246 246
  "",      // 247 247
  "",      // 248 248
  "",      // 249 249
  "",      // 250 250
  "",      // 251 251
  "",      // 252 252
  "",      // 253 253
  "",      // 254 254
  "",      // 255 255
  "",      // 256 256
  "",      // 257 257
  "",      // 258 258
  "",      // 259 259
  "",      // 260 260
  "",      // 261 261
  "",      // 262 262
  "",      // 263 263
  "",      // 264 264
  "",      // 265 265
  "",      // 266 266
  "",      // 267 267
  "",      // 268 268
  "",      // 269 269
  "",      // 270 270
  "",      // 271 271
  "",      // 272 272
  "",      // 273 273
  "",      // 274 274
  "",      // 275 275
  "",      // 276 276
  "",      // 277 277
  "",      // 278 278
  "",      // 279 279
  "",      // 280 280
  "",      // 281 281
  "",      // 282 282
  "",      // 283 283
  "",      // 284 284
  "",      // 285 285
  "",      // 286 286
  "",      // 287 287
  "",      // 288 288
  "",      // 289 289
  "",      // 290 290
  "",      // 291 291
  "",      // 292 292
  "",      // 293 293
  "",      // 294 294
  "",      // 295 295
  "",      // 296 296
  "",      // 297 297
  "",      // 298 298
  "",      // 299 299
  "",      // 300 300
  "",      // 301 301
  "",      // 302 302
  "",      // 303 303
  "",      // 304 304
  "",      // 305 305
  "",      // 306 306
  "",      // 307 307
  "",      // 308 308
  "",      // 309 309
  "",      // 310 310
  "",      // 311 311
  "",      // 312 312
  "",      // 313 313
  "",      // 314 314
  "",      // 315 315
  "",      // 316 316
  "",      // 317 317
  "",      // 318 318
  "",      // 319 319
  "",      // 320 320
  "",      // 321 321
  "",      // 322 322
  "",      // 323 323
  "",      // 324 324
  "",      // 325 325
  "",      // 326 326
  "",      // 327 327
  "",      // 328 328
  "",      // 329 329
  "",      // 330 330
  "",      // 331 331
  "",      // 332 332
  "",      // 333 333
  "",      // 334 334
  "",      // 335 335
  "",      // 336 336
  "",      // 337 337
  "",      // 338 338
  "",      // 339 339
  "",      // 340 340
  "",      // 341 341
  "",      // 342 342
  "",      // 343 343
  "",      // 344 344
  "",      // 345 345
  "",      // 346 346
  "",      // 347 347
  "",      // 348 348
  "",      // 349 349
  "",      // 350 350
  "",      // 351 351
  "",      // 352 352
  "",      // 353 353
  "",      // 354 354
  "",      // 355 355
  "",      // 356 356
  "",      // 357 357
  "",      // 358 358
  "",      // 359 359
  "",      // 360 360
  "",      // 361 361
  "",      // 362 362
  "",      // 363 363
  "",      // 364 364
  "",      // 365 365
  "",      // 366 366
  "",      // 367 367
  "",      // 368 368
  "",      // 369 369
  "",      // 370 370
  "",      // 371 371
  "",      // 372 372
  "",      // 373 373
  "",      // 374 374
  "",      // 375 375
  "",      // 376 376
  "",      // 377 377
  "",      // 378 378
  "",      // 379 379
  "",      // 380 380
  "",      // 381 381
  "",      // 382 382
  "",      // 383 383
  "",      // 384 384
  "",      // 385 385
  "",      // 386 386
  "",      // 387 387
  "",      // 388 388
  "",      // 389 389
  "",      // 390 390
  "",      // 391 391
  "",      // 392 392
  "",      // 393 393
  "",      // 394 394
  "",      // 395 395
  "",      // 396 396
  "",      // 397 397
  "",      // 398 398
  "",      // 399 399
  "",      // 400 400
  "",      // 401 401
  "",      // 402 402
  "",      // 403 403
  "",      // 404 404
  "",      // 405 405
  "",      // 406 406
  "",      // 407 407
  "",      // 408 408
  "",      // 409 409
  "",      // 410 410
  "",      // 411 411
  "",      // 412 412
  "",      // 413 413
  "",      // 414 414
  "",      // 415 415
  "",      // 416 416
  "",      // 417 417
  "",      // 418 418
  "",      // 419 419
  "",      // 420 420
  "",      // 421 421
  "",      // 422 422
  "",      // 423 423
  "",      // 424 424
  "",      // 425 425
  "",      // 426 426
  "",      // 427 427
  "",      // 428 428
  "",      // 429 429
  "",      // 430 430
  "",      // 431 431
  "",      // 432 432
  "",      // 433 433
  "",      // 434 434
  "",      // 435 435
  "",      // 436 436
  "",      // 437 437
  "",      // 438 438
  "",      // 439 439
  "",      // 440 440
  "",      // 441 441
  "",      // 442 442
  "",      // 443 443
  "",      // 444 444
  "",      // 445 445
  "",      // 446 446
  "",      // 447 447
  "",      // 448 448
  "",      // 449 449
  "",      // 450 450
  "",      // 451 451
  "",      // 452 452
  "",      // 453 453
  "",      // 454 454
  "",      // 455 455
  "",      // 456 456
  "",      // 457 457
  "",      // 458 458
  "",      // 459 459
  "",      // 460 460
  "",      // 461 461
  "",      // 462 462
  "",      // 463 463
  "",      // 464 464
  "",      // 465 465
  "",      // 466 466
  "",      // 467 467
  "",      // 468 468
  "",      // 469 469
  "",      // 470 470
  "",      // 471 471
  "",      // 472 472
  "",      // 473 473
  "",      // 474 474
  "",      // 475 475
  "",      // 476 476
  "",      // 477 477
  "",      // 478 478
  "",      // 479 479
  "",      // 480 480
  "",      // 481 481
  "",      // 482 482
  "",      // 483 483
  "",      // 484 484
  "",      // 485 485
  "",      // 486 486
  "",      // 487 487
  "",      // 488 488
  "",      // 489 489
  "",      // 490 490
  "",      // 491 491
  "",      // 492 492
  "",      // 493 493
  "",      // 494 494
  "",      // 495 495
  "",      // 496 496
  "",      // 497 497
  "",      // 498 498
  "",      // 499 499
  "",      // 500 500
  "",      // 501 501
  "",      // 502 502
  "",      // 503 503
  "",      // 504 504
  "",      // 505 505
  "nr",    // 506 NDEBELE
  "zzb",   // 507 X_BORK_BORK_BORK
  "zzp",   // 508 X_PIG_LATIN
  "zzh",   // 509 X_HACKER
  "tlh",   // 510 X_KLINGON
  "zze",   // 511 X_ELMER_FUDD
  "xx-Zyyy",  // 512 X_Common
  "xx-Latn",  // 513 X_Latin
  "xx-Grek",  // 514 X_Greek
  "xx-Cyrl",  // 515 X_Cyrillic
  "xx-Armn",  // 516 X_Armenian
  "xx-Hebr",  // 517 X_Hebrew
  "xx-Arab",  // 518 X_Arabic
  "xx-Syrc",  // 519 X_Syriac
  "xx-Thaa",  // 520 X_Thaana
  "xx-Deva",  // 521 X_Devanagari
  "xx-Beng",  // 522 X_Bengali
  "xx-Guru",  // 523 X_Gurmukhi
  "xx-Gujr",  // 524 X_Gujarati
  "xx-Orya",  // 525 X_Oriya
  "xx-Taml",  // 526 X_Tamil
  "xx-Telu",  // 527 X_Telugu
  "xx-Knda",  // 528 X_Kannada
  "xx-Mlym",  // 529 X_Malayalam
  "xx-Sinh",  // 530 X_Sinhala
  "xx-Thai",  // 531 X_Thai
  "xx-Laoo",  // 532 X_Lao
  "xx-Tibt",  // 533 X_Tibetan
  "xx-Mymr",  // 534 X_Myanmar
  "xx-Geor",  // 535 X_Georgian
  "xx-Hang",  // 536 X_Hangul
  "xx-Ethi",  // 537 X_Ethiopic
  "xx-Cher",  // 538 X_Cherokee
  "xx-Cans",  // 539 X_Canadian_Aboriginal
  "xx-Ogam",  // 540 X_Ogham
  "xx-Runr",  // 541 X_Runic
  "xx-Khmr",  // 542 X_Khmer
  "xx-Mong",  // 543 X_Mongolian
  "xx-Hira",  // 544 X_Hiragana
  "xx-Kana",  // 545 X_Katakana
  "xx-Bopo",  // 546 X_Bopomofo
  "xx-Hani",  // 547 X_Han
  "xx-Yiii",  // 548 X_Yi
  "xx-Ital",  // 549 X_Old_Italic
  "xx-Goth",  // 550 X_Gothic
  "xx-Dsrt",  // 551 X_Deseret
  "xx-Qaai",  // 552 X_Inherited
  "xx-Tglg",  // 553 X_Tagalog
  "xx-Hano",  // 554 X_Hanunoo
  "xx-Buhd",  // 555 X_Buhid
  "xx-Tagb",  // 556 X_Tagbanwa
  "xx-Limb",  // 557 X_Limbu
  "xx-Tale",  // 558 X_Tai_Le
  "xx-Linb",  // 559 X_Linear_B
  "xx-Ugar",  // 560 X_Ugaritic
  "xx-Shaw",  // 561 X_Shavian
  "xx-Osma",  // 562 X_Osmanya
  "xx-Cprt",  // 563 X_Cypriot
  "xx-Brai",  // 564 X_Braille
  "xx-Bugi",  // 565 X_Buginese
  "xx-Copt",  // 566 X_Coptic
  "xx-Talu",  // 567 X_New_Tai_Lue
  "xx-Glag",  // 568 X_Glagolitic
  "xx-Tfng",  // 569 X_Tifinagh
  "xx-Sylo",  // 570 X_Syloti_Nagri
  "xx-Xpeo",  // 571 X_Old_Persian
  "xx-Khar",  // 572 X_Kharoshthi
  "xx-Bali",  // 573 X_Balinese
  "xx-Xsux",  // 574 X_Cuneiform
  "xx-Phnx",  // 575 X_Phoenician
  "xx-Phag",  // 576 X_Phags_Pa
  "xx-Nkoo",  // 577 X_Nko
  "xx-Sund",  // 578 X_Sundanese
  "xx-Lepc",  // 579 X_Lepcha
  "xx-Olck",  // 580 X_Ol_Chiki
  "xx-Vaii",  // 581 X_Vai
  "xx-Saur",  // 582 X_Saurashtra
  "xx-Kali",  // 583 X_Kayah_Li
  "xx-Rjng",  // 584 X_Rejang
  "xx-Lyci",  // 585 X_Lycian
  "xx-Cari",  // 586 X_Carian
  "xx-Lydi",  // 587 X_Lydian
  "xx-Cham",  // 588 X_Cham
  "xx-Lana",  // 589 X_Tai_Tham
  "xx-Tavt",  // 590 X_Tai_Viet
  "xx-Avst",  // 591 X_Avestan
  "xx-Egyp",  // 592 X_Egyptian_Hieroglyphs
  "xx-Samr",  // 593 X_Samaritan
  "xx-Lisu",  // 594 X_Lisu
  "xx-Bamu",  // 595 X_Bamum
  "xx-Java",  // 596 X_Javanese
  "xx-Mtei",  // 597 X_Meetei_Mayek
  "xx-Armi",  // 598 X_Imperial_Aramaic
  "xx-Sarb",  // 599 X_Old_South_Arabian
  "xx-Prti",  // 600 X_Inscriptional_Parthian
  "xx-Phli",  // 601 X_Inscriptional_Pahlavi
  "xx-Orkh",  // 602 X_Old_Turkic
  "xx-Kthi",  // 603 X_Kaithi
  "xx-Batk",  // 604 X_Batak
  "xx-Brah",  // 605 X_Brahmi
  "xx-Mand",  // 606 X_Mandaic
  "xx-Cakm",  // 607 X_Chakma
  "xx-Merc",  // 608 X_Meroitic_Cursive
  "xx-Mero",  // 609 X_Meroitic_Hieroglyphs
  "xx-Plrd",  // 610 X_Miao
  "xx-Shrd",  // 611 X_Sharada
  "xx-Sora",  // 612 X_Sora_Sompeng
  "xx-Takr",  // 613 X_Takri
};

// Subscripted by enum Language
extern const int kLanguageToCNameSize = 614;
extern const char* const kLanguageToCName[kLanguageToCNameSize] = {
  "ENGLISH",               // 0 en
  "DANISH",                // 1 da
  "DUTCH",                 // 2 nl
  "FINNISH",               // 3 fi
  "FRENCH",                // 4 fr
  "GERMAN",                // 5 de
  "HEBREW",                // 6 iw
  "ITALIAN",               // 7 it
  "JAPANESE",              // 8 ja
  "KOREAN",                // 9 ko
  "NORWEGIAN",             // 10 no
  "POLISH",                // 11 pl
  "PORTUGUESE",            // 12 pt
  "RUSSIAN",               // 13 ru
  "SPANISH",               // 14 es
  "SWEDISH",               // 15 sv
  "CHINESE",               // 16 zh
  "CZECH",                 // 17 cs
  "GREEK",                 // 18 el
  "ICELANDIC",             // 19 is
  "LATVIAN",               // 20 lv
  "LITHUANIAN",            // 21 lt
  "ROMANIAN",              // 22 ro
  "HUNGARIAN",             // 23 hu
  "ESTONIAN",              // 24 et
  "TG_UNKNOWN_LANGUAGE",   // 25 xxx
  "UNKNOWN_LANGUAGE",      // 26 un
  "BULGARIAN",             // 27 bg
  "CROATIAN",              // 28 hr
  "SERBIAN",               // 29 sr
  "IRISH",                 // 30 ga
  "GALICIAN",              // 31 gl
  "TAGALOG",               // 32 tl
  "TURKISH",               // 33 tr
  "UKRAINIAN",             // 34 uk
  "HINDI",                 // 35 hi
  "MACEDONIAN",            // 36 mk
  "BENGALI",               // 37 bn
  "INDONESIAN",            // 38 id
  "LATIN",                 // 39 la
  "MALAY",                 // 40 ms
  "MALAYALAM",             // 41 ml
  "WELSH",                 // 42 cy
  "NEPALI",                // 43 ne
  "TELUGU",                // 44 te
  "ALBANIAN",              // 45 sq
  "TAMIL",                 // 46 ta
  "BELARUSIAN",            // 47 be
  "JAVANESE",              // 48 jw
  "OCCITAN",               // 49 oc
  "URDU",                  // 50 ur
  "BIHARI",                // 51 bh
  "GUJARATI",              // 52 gu
  "THAI",                  // 53 th
  "ARABIC",                // 54 ar
  "CATALAN",               // 55 ca
  "ESPERANTO",             // 56 eo
  "BASQUE",                // 57 eu
  "INTERLINGUA",           // 58 ia
  "KANNADA",               // 59 kn
  "PUNJABI",               // 60 pa
  "SCOTS_GAELIC",          // 61 gd
  "SWAHILI",               // 62 sw
  "SLOVENIAN",             // 63 sl
  "MARATHI",               // 64 mr
  "MALTESE",               // 65 mt
  "VIETNAMESE",            // 66 vi
  "FRISIAN",               // 67 fy
  "SLOVAK",                // 68 sk
  "CHINESE_T",             // 69 zh-Hant
  "FAROESE",               // 70 fo
  "SUNDANESE",             // 71 su
  "UZBEK",                 // 72 uz
  "AMHARIC",               // 73 am
  "AZERBAIJANI",           // 74 az
  "GEORGIAN",              // 75 ka
  "TIGRINYA",              // 76 ti
  "PERSIAN",               // 77 fa
  "BOSNIAN",               // 78 bs
  "SINHALESE",             // 79 si
  "NORWEGIAN_N",           // 80 nn
  "X_81",                  // 81
  "X_82",                  // 82
  "XHOSA",                 // 83 xh
  "ZULU",                  // 84 zu
  "GUARANI",               // 85 gn
  "SESOTHO",               // 86 st
  "TURKMEN",               // 87 tk
  "KYRGYZ",                // 88 ky
  "BRETON",                // 89 br
  "TWI",                   // 90 tw
  "YIDDISH",               // 91 yi
  "X_92",                  // 92
  "SOMALI",                // 93 so
  "UIGHUR",                // 94 ug
  "KURDISH",               // 95 ku
  "MONGOLIAN",             // 96 mn
  "ARMENIAN",              // 97 hy
  "LAOTHIAN",              // 98 lo
  "SINDHI",                // 99 sd
  "RHAETO_ROMANCE",        // 100 rm
  "AFRIKAANS",             // 101 af
  "LUXEMBOURGISH",         // 102 lb
  "BURMESE",               // 103 my
  "KHMER",                 // 104 km
  "TIBETAN",               // 105 bo
  "DHIVEHI",               // 106 dv
  "CHEROKEE",              // 107 chr
  "SYRIAC",                // 108 syr
  "LIMBU",                 // 109 lif
  "ORIYA",                 // 110 or
  "ASSAMESE",              // 111 as
  "CORSICAN",              // 112 co
  "INTERLINGUE",           // 113 ie
  "KAZAKH",                // 114 kk
  "LINGALA",               // 115 ln
  "X_116",                 // 116
  "PASHTO",                // 117 ps
  "QUECHUA",               // 118 qu
  "SHONA",                 // 119 sn
  "TAJIK",                 // 120 tg
  "TATAR",                 // 121 tt
  "TONGA",                 // 122 to
  "YORUBA",                // 123 yo
  "X_124",                 // 124
  "X_125",                 // 125
  "X_126",                 // 126
  "X_127",                 // 127
  "MAORI",                 // 128 mi
  "WOLOF",                 // 129 wo
  "ABKHAZIAN",             // 130 ab
  "AFAR",                  // 131 aa
  "AYMARA",                // 132 ay
  "BASHKIR",               // 133 ba
  "BISLAMA",               // 134 bi
  "DZONGKHA",              // 135 dz
  "FIJIAN",                // 136 fj
  "GREENLANDIC",           // 137 kl
  "HAUSA",                 // 138 ha
  "HAITIAN_CREOLE",        // 139 ht
  "INUPIAK",               // 140 ik
  "INUKTITUT",             // 141 iu
  "KASHMIRI",              // 142 ks
  "KINYARWANDA",           // 143 rw
  "MALAGASY",              // 144 mg
  "NAURU",                 // 145 na
  "OROMO",                 // 146 om
  "RUNDI",                 // 147 rn
  "SAMOAN",                // 148 sm
  "SANGO",                 // 149 sg
  "SANSKRIT",              // 150 sa
  "SISWANT",               // 151 ss
  "TSONGA",                // 152 ts
  "TSWANA",                // 153 tn
  "VOLAPUK",               // 154 vo
  "ZHUANG",                // 155 za
  "KHASI",                 // 156 kha
  "SCOTS",                 // 157 sco
  "GANDA",                 // 158 lg
  "MANX",                  // 159 gv
  "MONTENEGRIN",           // 160 sr-ME
  "AKAN",                  // 161 ak
  "IGBO",                  // 162 ig
  "MAURITIAN_CREOLE",      // 163 mfe
  "HAWAIIAN",              // 164 haw
  "CEBUANO",               // 165 ceb
  "EWE",                   // 166 ee
  "GA",                    // 167 gaa
  "HMONG",                 // 168 hmn
  "KRIO",                  // 169 kri
  "LOZI",                  // 170 loz
  "LUBA_LULUA",            // 171 lua
  "LUO_KENYA_AND_TANZANIA",  // 172 luo
  "NEWARI",                // 173 new
  "NYANJA",                // 174 ny
  "OSSETIAN",              // 175 os
  "PAMPANGA",              // 176 pam
  "PEDI",                  // 177 nso
  "RAJASTHANI",            // 178 raj
  "SESELWA",               // 179 crs
  "TUMBUKA",               // 180 tum
  "VENDA",                 // 181 ve
  "WARAY_PHILIPPINES",     // 182 war
  "X_183",                 // 183
  "X_184",                 // 184
  "X_185",                 // 185
  "X_186",                 // 186
  "X_187",                 // 187
  "X_188",                 // 188
  "X_189",                 // 189
  "X_190",                 // 190
  "X_191",                 // 191
  "X_192",                 // 192
  "X_193",                 // 193
  "X_194",                 // 194
  "X_195",                 // 195
  "X_196",                 // 196
  "X_197",                 // 197
  "X_198",                 // 198
  "X_199",                 // 199
  "X_200",                 // 200
  "X_201",                 // 201
  "X_202",                 // 202
  "X_203",                 // 203
  "X_204",                 // 204
  "X_205",                 // 205
  "X_206",                 // 206
  "X_207",                 // 207
  "X_208",                 // 208
  "X_209",                 // 209
  "X_210",                 // 210
  "X_211",                 // 211
  "X_212",                 // 212
  "X_213",                 // 213
  "X_214",                 // 214
  "X_215",                 // 215
  "X_216",                 // 216
  "X_217",                 // 217
  "X_218",                 // 218
  "X_219",                 // 219
  "X_220",                 // 220
  "X_221",                 // 221
  "X_222",                 // 222
  "X_223",                 // 223
  "X_224",                 // 224
  "X_225",                 // 225
  "X_226",                 // 226
  "X_227",                 // 227
  "X_228",                 // 228
  "X_229",                 // 229
  "X_230",                 // 230
  "X_231",                 // 231
  "X_232",                 // 232
  "X_233",                 // 233
  "X_234",                 // 234
  "X_235",                 // 235
  "X_236",                 // 236
  "X_237",                 // 237
  "X_238",                 // 238
  "X_239",                 // 239
  "X_240",                 // 240
  "X_241",                 // 241
  "X_242",                 // 242
  "X_243",                 // 243
  "X_244",                 // 244
  "X_245",                 // 245
  "X_246",                 // 246
  "X_247",                 // 247
  "X_248",                 // 248
  "X_249",                 // 249
  "X_250",                 // 250
  "X_251",                 // 251
  "X_252",                 // 252
  "X_253",                 // 253
  "X_254",                 // 254
  "X_255",                 // 255
  "X_256",                 // 256
  "X_257",                 // 257
  "X_258",                 // 258
  "X_259",                 // 259
  "X_260",                 // 260
  "X_261",                 // 261
  "X_262",                 // 262
  "X_263",                 // 263
  "X_264",                 // 264
  "X_265",                 // 265
  "X_266",                 // 266
  "X_267",                 // 267
  "X_268",                 // 268
  "X_269",                 // 269
  "X_270",                 // 270
  "X_271",                 // 271
  "X_272",                 // 272
  "X_273",                 // 273
  "X_274",                 // 274
  "X_275",                 // 275
  "X_276",                 // 276
  "X_277",                 // 277
  "X_278",                 // 278
  "X_279",                 // 279
  "X_280",                 // 280
  "X_281",                 // 281
  "X_282",                 // 282
  "X_283",                 // 283
  "X_284",                 // 284
  "X_285",                 // 285
  "X_286",                 // 286
  "X_287",                 // 287
  "X_288",                 // 288
  "X_289",                 // 289
  "X_290",                 // 290
  "X_291",                 // 291
  "X_292",                 // 292
  "X_293",                 // 293
  "X_294",                 // 294
  "X_295",                 // 295
  "X_296",                 // 296
  "X_297",                 // 297
  "X_298",                 // 298
  "X_299",                 // 299
  "X_300",                 // 300
  "X_301",                 // 301
  "X_302",                 // 302
  "X_303",                 // 303
  "X_304",                 // 304
  "X_305",                 // 305
  "X_306",                 // 306
  "X_307",                 // 307
  "X_308",                 // 308
  "X_309",                 // 309
  "X_310",                 // 310
  "X_311",                 // 311
  "X_312",                 // 312
  "X_313",                 // 313
  "X_314",                 // 314
  "X_315",                 // 315
  "X_316",                 // 316
  "X_317",                 // 317
  "X_318",                 // 318
  "X_319",                 // 319
  "X_320",                 // 320
  "X_321",                 // 321
  "X_322",                 // 322
  "X_323",                 // 323
  "X_324",                 // 324
  "X_325",                 // 325
  "X_326",                 // 326
  "X_327",                 // 327
  "X_328",                 // 328
  "X_329",                 // 329
  "X_330",                 // 330
  "X_331",                 // 331
  "X_332",                 // 332
  "X_333",                 // 333
  "X_334",                 // 334
  "X_335",                 // 335
  "X_336",                 // 336
  "X_337",                 // 337
  "X_338",                 // 338
  "X_339",                 // 339
  "X_340",                 // 340
  "X_341",                 // 341
  "X_342",                 // 342
  "X_343",                 // 343
  "X_344",                 // 344
  "X_345",                 // 345
  "X_346",                 // 346
  "X_347",                 // 347
  "X_348",                 // 348
  "X_349",                 // 349
  "X_350",                 // 350
  "X_351",                 // 351
  "X_352",                 // 352
  "X_353",                 // 353
  "X_354",                 // 354
  "X_355",                 // 355
  "X_356",                 // 356
  "X_357",                 // 357
  "X_358",                 // 358
  "X_359",                 // 359
  "X_360",                 // 360
  "X_361",                 // 361
  "X_362",                 // 362
  "X_363",                 // 363
  "X_364",                 // 364
  "X_365",                 // 365
  "X_366",                 // 366
  "X_367",                 // 367
  "X_368",                 // 368
  "X_369",                 // 369
  "X_370",                 // 370
  "X_371",                 // 371
  "X_372",                 // 372
  "X_373",                 // 373
  "X_374",                 // 374
  "X_375",                 // 375
  "X_376",                 // 376
  "X_377",                 // 377
  "X_378",                 // 378
  "X_379",                 // 379
  "X_380",                 // 380
  "X_381",                 // 381
  "X_382",                 // 382
  "X_383",                 // 383
  "X_384",                 // 384
  "X_385",                 // 385
  "X_386",                 // 386
  "X_387",                 // 387
  "X_388",                 // 388
  "X_389",                 // 389
  "X_390",                 // 390
  "X_391",                 // 391
  "X_392",                 // 392
  "X_393",                 // 393
  "X_394",                 // 394
  "X_395",                 // 395
  "X_396",                 // 396
  "X_397",                 // 397
  "X_398",                 // 398
  "X_399",                 // 399
  "X_400",                 // 400
  "X_401",                 // 401
  "X_402",                 // 402
  "X_403",                 // 403
  "X_404",                 // 404
  "X_405",                 // 405
  "X_406",                 // 406
  "X_407",                 // 407
  "X_408",                 // 408
  "X_409",                 // 409
  "X_410",                 // 410
  "X_411",                 // 411
  "X_412",                 // 412
  "X_413",                 // 413
  "X_414",                 // 414
  "X_415",                 // 415
  "X_416",                 // 416
  "X_417",                 // 417
  "X_418",                 // 418
  "X_419",                 // 419
  "X_420",                 // 420
  "X_421",                 // 421
  "X_422",                 // 422
  "X_423",                 // 423
  "X_424",                 // 424
  "X_425",                 // 425
  "X_426",                 // 426
  "X_427",                 // 427
  "X_428",                 // 428
  "X_429",                 // 429
  "X_430",                 // 430
  "X_431",                 // 431
  "X_432",                 // 432
  "X_433",                 // 433
  "X_434",                 // 434
  "X_435",                 // 435
  "X_436",                 // 436
  "X_437",                 // 437
  "X_438",                 // 438
  "X_439",                 // 439
  "X_440",                 // 440
  "X_441",                 // 441
  "X_442",                 // 442
  "X_443",                 // 443
  "X_444",                 // 444
  "X_445",                 // 445
  "X_446",                 // 446
  "X_447",                 // 447
  "X_448",                 // 448
  "X_449",                 // 449
  "X_450",                 // 450
  "X_451",                 // 451
  "X_452",                 // 452
  "X_453",                 // 453
  "X_454",                 // 454
  "X_455",                 // 455
  "X_456",                 // 456
  "X_457",                 // 457
  "X_458",                 // 458
  "X_459",                 // 459
  "X_460",                 // 460
  "X_461",                 // 461
  "X_462",                 // 462
  "X_463",                 // 463
  "X_464",                 // 464
  "X_465",                 // 465
  "X_466",                 // 466
  "X_467",                 // 467
  "X_468",                 // 468
  "X_469",                 // 469
  "X_470",                 // 470
  "X_471",                 // 471
  "X_472",                 // 472
  "X_473",                 // 473
  "X_474",                 // 474
  "X_475",                 // 475
  "X_476",                 // 476
  "X_477",                 // 477
  "X_478",                 // 478
  "X_479",                 // 479
  "X_480",                 // 480
  "X_481",                 // 481
  "X_482",                 // 482
  "X_483",                 // 483
  "X_484",                 // 484
  "X_485",                 // 485
  "X_486",                 // 486
  "X_487",                 // 487
  "X_488",                 // 488
  "X_489",                 // 489
  "X_490",                 // 490
  "X_491",                 // 491
  "X_492",                 // 492
  "X_493",                 // 493
  "X_494",                 // 494
  "X_495",                 // 495
  "X_496",                 // 496
  "X_497",                 // 497
  "X_498",                 // 498
  "X_499",                 // 499
  "X_500",                 // 500
  "X_501",                 // 501
  "X_502",                 // 502
  "X_503",                 // 503
  "X_504",                 // 504
  "X_505",                 // 505
  "NDEBELE",               // 506 nr
  "X_BORK_BORK_BORK",      // 507 zzb
  "X_PIG_LATIN",           // 508 zzp
  "X_HACKER",              // 509 zzh
  "X_KLINGON",             // 510 tlh
  "X_ELMER_FUDD",          // 511 zze
  "X_Common",              // 512 xx-Zyyy
  "X_Latin",               // 513 xx-Latn
  "X_Greek",               // 514 xx-Grek
  "X_Cyrillic",            // 515 xx-Cyrl
  "X_Armenian",            // 516 xx-Armn
  "X_Hebrew",              // 517 xx-Hebr
  "X_Arabic",              // 518 xx-Arab
  "X_Syriac",              // 519 xx-Syrc
  "X_Thaana",              // 520 xx-Thaa
  "X_Devanagari",          // 521 xx-Deva
  "X_Bengali",             // 522 xx-Beng
  "X_Gurmukhi",            // 523 xx-Guru
  "X_Gujarati",            // 524 xx-Gujr
  "X_Oriya",               // 525 xx-Orya
  "X_Tamil",               // 526 xx-Taml
  "X_Telugu",              // 527 xx-Telu
  "X_Kannada",             // 528 xx-Knda
  "X_Malayalam",           // 529 xx-Mlym
  "X_Sinhala",             // 530 xx-Sinh
  "X_Thai",                // 531 xx-Thai
  "X_Lao",                 // 532 xx-Laoo
  "X_Tibetan",             // 533 xx-Tibt
  "X_Myanmar",             // 534 xx-Mymr
  "X_Georgian",            // 535 xx-Geor
  "X_Hangul",              // 536 xx-Hang
  "X_Ethiopic",            // 537 xx-Ethi
  "X_Cherokee",            // 538 xx-Cher
  "X_Canadian_Aboriginal",  // 539 xx-Cans
  "X_Ogham",               // 540 xx-Ogam
  "X_Runic",               // 541 xx-Runr
  "X_Khmer",               // 542 xx-Khmr
  "X_Mongolian",           // 543 xx-Mong
  "X_Hiragana",            // 544 xx-Hira
  "X_Katakana",            // 545 xx-Kana
  "X_Bopomofo",            // 546 xx-Bopo
  "X_Han",                 // 547 xx-Hani
  "X_Yi",                  // 548 xx-Yiii
  "X_Old_Italic",          // 549 xx-Ital
  "X_Gothic",              // 550 xx-Goth
  "X_Deseret",             // 551 xx-Dsrt
  "X_Inherited",           // 552 xx-Qaai
  "X_Tagalog",             // 553 xx-Tglg
  "X_Hanunoo",             // 554 xx-Hano
  "X_Buhid",               // 555 xx-Buhd
  "X_Tagbanwa",            // 556 xx-Tagb
  "X_Limbu",               // 557 xx-Limb
  "X_Tai_Le",              // 558 xx-Tale
  "X_Linear_B",            // 559 xx-Linb
  "X_Ugaritic",            // 560 xx-Ugar
  "X_Shavian",             // 561 xx-Shaw
  "X_Osmanya",             // 562 xx-Osma
  "X_Cypriot",             // 563 xx-Cprt
  "X_Braille",             // 564 xx-Brai
  "X_Buginese",            // 565 xx-Bugi
  "X_Coptic",              // 566 xx-Copt
  "X_New_Tai_Lue",         // 567 xx-Talu
  "X_Glagolitic",          // 568 xx-Glag
  "X_Tifinagh",            // 569 xx-Tfng
  "X_Syloti_Nagri",        // 570 xx-Sylo
  "X_Old_Persian",         // 571 xx-Xpeo
  "X_Kharoshthi",          // 572 xx-Khar
  "X_Balinese",            // 573 xx-Bali
  "X_Cuneiform",           // 574 xx-Xsux
  "X_Phoenician",          // 575 xx-Phnx
  "X_Phags_Pa",            // 576 xx-Phag
  "X_Nko",                 // 577 xx-Nkoo
  "X_Sundanese",           // 578 xx-Sund
  "X_Lepcha",              // 579 xx-Lepc
  "X_Ol_Chiki",            // 580 xx-Olck
  "X_Vai",                 // 581 xx-Vaii
  "X_Saurashtra",          // 582 xx-Saur
  "X_Kayah_Li",            // 583 xx-Kali
  "X_Rejang",              // 584 xx-Rjng
  "X_Lycian",              // 585 xx-Lyci
  "X_Carian",              // 586 xx-Cari
  "X_Lydian",              // 587 xx-Lydi
  "X_Cham",                // 588 xx-Cham
  "X_Tai_Tham",            // 589 xx-Lana
  "X_Tai_Viet",            // 590 xx-Tavt
  "X_Avestan",             // 591 xx-Avst
  "X_Egyptian_Hieroglyphs",  // 592 xx-Egyp
  "X_Samaritan",           // 593 xx-Samr
  "X_Lisu",                // 594 xx-Lisu
  "X_Bamum",               // 595 xx-Bamu
  "X_Javanese",            // 596 xx-Java
  "X_Meetei_Mayek",        // 597 xx-Mtei
  "X_Imperial_Aramaic",    // 598 xx-Armi
  "X_Old_South_Arabian",   // 599 xx-Sarb
  "X_Inscriptional_Parthian",  // 600 xx-Prti
  "X_Inscriptional_Pahlavi",  // 601 xx-Phli
  "X_Old_Turkic",          // 602 xx-Orkh
  "X_Kaithi",              // 603 xx-Kthi
  "X_Batak",               // 604 xx-Batk
  "X_Brahmi",              // 605 xx-Brah
  "X_Mandaic",             // 606 xx-Mand
  "X_Chakma",              // 607 xx-Cakm
  "X_Meroitic_Cursive",    // 608 xx-Merc
  "X_Meroitic_Hieroglyphs",  // 609 xx-Mero
  "X_Miao",                // 610 xx-Plrd
  "X_Sharada",             // 611 xx-Shrd
  "X_Sora_Sompeng",        // 612 xx-Sora
  "X_Takri",               // 613 xx-Takr
};

// Subscripted by enum Language
extern const int kLanguageToScriptsSize = 614;
#define None ULScript_Common
extern const FourScripts kLanguageToScripts[kLanguageToScriptsSize] = {
  {ULScript_Latin, None, None, None, },  // 0 en
  {ULScript_Latin, None, None, None, },  // 1 da
  {ULScript_Latin, None, None, None, },  // 2 nl
  {ULScript_Latin, None, None, None, },  // 3 fi
  {ULScript_Latin, None, None, None, },  // 4 fr
  {ULScript_Latin, None, None, None, },  // 5 de
  {ULScript_Hebrew, None, None, None, },  // 6 iw
  {ULScript_Latin, None, None, None, },  // 7 it
  {ULScript_Hani, None, None, None, },  // 8 ja
  {ULScript_Hani, None, None, None, },  // 9 ko
  {ULScript_Latin, None, None, None, },  // 10 no
  {ULScript_Latin, None, None, None, },  // 11 pl
  {ULScript_Latin, None, None, None, },  // 12 pt
  {ULScript_Cyrillic, None, None, None, },  // 13 ru
  {ULScript_Latin, None, None, None, },  // 14 es
  {ULScript_Latin, None, None, None, },  // 15 sv
  {ULScript_Hani, None, None, None, },  // 16 zh
  {ULScript_Latin, None, None, None, },  // 17 cs
  {ULScript_Greek, None, None, None, },  // 18 el
  {ULScript_Latin, None, None, None, },  // 19 is
  {ULScript_Latin, None, None, None, },  // 20 lv
  {ULScript_Latin, None, None, None, },  // 21 lt
  {ULScript_Latin, ULScript_Cyrillic, None, None, },  // 22 ro
  {ULScript_Latin, None, None, None, },  // 23 hu
  {ULScript_Latin, None, None, None, },  // 24 et
  {ULScript_Latin, ULScript_Cyrillic, ULScript_Arabic, ULScript_Devanagari, },  // 25 xxx
  {ULScript_Latin, None, None, None, },  // 26 un
  {ULScript_Cyrillic, None, None, None, },  // 27 bg
  {ULScript_Latin, None, None, None, },  // 28 hr
  {ULScript_Latin, ULScript_Cyrillic, None, None, },  // 29 sr
  {ULScript_Latin, None, None, None, },  // 30 ga
  {ULScript_Latin, None, None, None, },  // 31 gl
  {ULScript_Latin, ULScript_Tagalog, None, None, },  // 32 tl
  {ULScript_Latin, None, None, None, },  // 33 tr
  {ULScript_Cyrillic, None, None, None, },  // 34 uk
  {ULScript_Devanagari, None, None, None, },  // 35 hi
  {ULScript_Cyrillic, None, None, None, },  // 36 mk
  {ULScript_Bengali, None, None, None, },  // 37 bn
  {ULScript_Latin, None, None, None, },  // 38 id
  {ULScript_Latin, None, None, None, },  // 39 la
  {ULScript_Latin, None, None, None, },  // 40 ms
  {ULScript_Malayalam, None, None, None, },  // 41 ml
  {ULScript_Latin, None, None, None, },  // 42 cy
  {ULScript_Devanagari, None, None, None, },  // 43 ne
  {ULScript_Telugu, None, None, None, },  // 44 te
  {ULScript_Latin, None, None, None, },  // 45 sq
  {ULScript_Tamil, None, None, None, },  // 46 ta
  {ULScript_Cyrillic, None, None, None, },  // 47 be
  {ULScript_Latin, None, None, None, },  // 48 jw
  {ULScript_Latin, None, None, None, },  // 49 oc
  {ULScript_Arabic, None, None, None, },  // 50 ur
  {ULScript_Devanagari, None, None, None, },  // 51 bh
  {ULScript_Gujarati, None, None, None, },  // 52 gu
  {ULScript_Thai, None, None, None, },  // 53 th
  {ULScript_Arabic, None, None, None, },  // 54 ar
  {ULScript_Latin, None, None, None, },  // 55 ca
  {ULScript_Latin, None, None, None, },  // 56 eo
  {ULScript_Latin, None, None, None, },  // 57 eu
  {ULScript_Latin, None, None, None, },  // 58 ia
  {ULScript_Kannada, None, None, None, },  // 59 kn
  {ULScript_Gurmukhi, None, None, None, },  // 60 pa
  {ULScript_Latin, None, None, None, },  // 61 gd
  {ULScript_Latin, None, None, None, },  // 62 sw
  {ULScript_Latin, None, None, None, },  // 63 sl
  {ULScript_Devanagari, None, None, None, },  // 64 mr
  {ULScript_Latin, None, None, None, },  // 65 mt
  {ULScript_Latin, None, None, None, },  // 66 vi
  {ULScript_Latin, None, None, None, },  // 67 fy
  {ULScript_Latin, None, None, None, },  // 68 sk
  {ULScript_Hani, None, None, None, },  // 69 zh-Hant
  {ULScript_Latin, None, None, None, },  // 70 fo
  {ULScript_Latin, None, None, None, },  // 71 su
  {ULScript_Latin, ULScript_Cyrillic, ULScript_Arabic, None, },  // 72 uz
  {ULScript_Ethiopic, None, None, None, },  // 73 am
  {ULScript_Latin, ULScript_Cyrillic, ULScript_Arabic, None, },  // 74 az
  {ULScript_Georgian, None, None, None, },  // 75 ka
  {ULScript_Ethiopic, None, None, None, },  // 76 ti
  {ULScript_Arabic, None, None, None, },  // 77 fa
  {ULScript_Latin, ULScript_Cyrillic, None, None, },  // 78 bs
  {ULScript_Sinhala, None, None, None, },  // 79 si
  {ULScript_Latin, None, None, None, },  // 80 nn
  {None, None, None, None, },  // 81
  {None, None, None, None, },  // 82
  {ULScript_Latin, None, None, None, },  // 83 xh
  {ULScript_Latin, None, None, None, },  // 84 zu
  {ULScript_Latin, None, None, None, },  // 85 gn
  {ULScript_Latin, None, None, None, },  // 86 st
  {ULScript_Latin, ULScript_Cyrillic, ULScript_Arabic, None, },  // 87 tk
  {ULScript_Cyrillic, ULScript_Arabic, None, None, },  // 88 ky
  {ULScript_Latin, None, None, None, },  // 89 br
  {ULScript_Latin, None, None, None, },  // 90 tw
  {ULScript_Hebrew, None, None, None, },  // 91 yi
  {None, None, None, None, },  // 92
  {ULScript_Latin, None, None, None, },  // 93 so
  {ULScript_Latin, ULScript_Cyrillic, ULScript_Arabic, None, },  // 94 ug
  {ULScript_Latin, ULScript_Arabic, None, None, },  // 95 ku
  {ULScript_Cyrillic, ULScript_Mongolian, None, None, },  // 96 mn
  {ULScript_Armenian, None, None, None, },  // 97 hy
  {ULScript_Lao, None, None, None, },  // 98 lo
  {ULScript_Arabic, ULScript_Devanagari, None, None, },  // 99 sd
  {ULScript_Latin, None, None, None, },  // 100 rm
  {ULScript_Latin, None, None, None, },  // 101 af
  {ULScript_Latin, None, None, None, },  // 102 lb
  {ULScript_Latin, ULScript_Myanmar, None, None, },  // 103 my
  {ULScript_Khmer, None, None, None, },  // 104 km
  {ULScript_Tibetan, None, None, None, },  // 105 bo
  {ULScript_Thaana, None, None, None, },  // 106 dv
  {ULScript_Cherokee, None, None, None, },  // 107 chr
  {ULScript_Syriac, None, None, None, },  // 108 syr
  {ULScript_Limbu, None, None, None, },  // 109 lif
  {ULScript_Oriya, None, None, None, },  // 110 or
  {ULScript_Bengali, None, None, None, },  // 111 as
  {ULScript_Latin, None, None, None, },  // 112 co
  {ULScript_Latin, None, None, None, },  // 113 ie
  {ULScript_Latin, ULScript_Cyrillic, ULScript_Arabic, None, },  // 114 kk
  {ULScript_Latin, None, None, None, },  // 115 ln
  {None, None, None, None, },  // 116
  {ULScript_Arabic, None, None, None, },  // 117 ps
  {ULScript_Latin, None, None, None, },  // 118 qu
  {ULScript_Latin, None, None, None, },  // 119 sn
  {ULScript_Cyrillic, ULScript_Arabic, None, None, },  // 120 tg
  {ULScript_Latin, ULScript_Cyrillic, ULScript_Arabic, None, },  // 121 tt
  {ULScript_Latin, None, None, None, },  // 122 to
  {ULScript_Latin, None, None, None, },  // 123 yo
  {None, None, None, None, },  // 124
  {None, None, None, None, },  // 125
  {None, None, None, None, },  // 126
  {None, None, None, None, },  // 127
  {ULScript_Latin, None, None, None, },  // 128 mi
  {ULScript_Latin, None, None, None, },  // 129 wo
  {ULScript_Cyrillic, None, None, None, },  // 130 ab
  {ULScript_Latin, None, None, None, },  // 131 aa
  {ULScript_Latin, None, None, None, },  // 132 ay
  {ULScript_Cyrillic, None, None, None, },  // 133 ba
  {ULScript_Latin, None, None, None, },  // 134 bi
  {ULScript_Tibetan, None, None, None, },  // 135 dz
  {ULScript_Latin, None, None, None, },  // 136 fj
  {ULScript_Latin, None, None, None, },  // 137 kl
  {ULScript_Latin, ULScript_Arabic, None, None, },  // 138 ha
  {ULScript_Latin, None, None, None, },  // 139 ht
  {ULScript_Latin, None, None, None, },  // 140 ik
  {ULScript_Canadian_Aboriginal, None, None, None, },  // 141 iu
  {ULScript_Arabic, ULScript_Devanagari, None, None, },  // 142 ks
  {ULScript_Latin, None, None, None, },  // 143 rw
  {ULScript_Latin, None, None, None, },  // 144 mg
  {ULScript_Latin, None, None, None, },  // 145 na
  {ULScript_Latin, None, None, None, },  // 146 om
  {ULScript_Latin, None, None, None, },  // 147 rn
  {ULScript_Latin, None, None, None, },  // 148 sm
  {ULScript_Latin, None, None, None, },  // 149 sg
  {ULScript_Latin, ULScript_Devanagari, None, None, },  // 150 sa
  {ULScript_Latin, None, None, None, },  // 151 ss
  {ULScript_Latin, None, None, None, },  // 152 ts
  {ULScript_Latin, None, None, None, },  // 153 tn
  {ULScript_Latin, None, None, None, },  // 154 vo
  {ULScript_Latin, ULScript_Hani, None, None, },  // 155 za
  {ULScript_Latin, None, None, None, },  // 156 kha
  {ULScript_Latin, None, None, None, },  // 157 sco
  {ULScript_Latin, None, None, None, },  // 158 lg
  {ULScript_Latin, None, None, None, },  // 159 gv
  {ULScript_Latin, None, None, None, },  // 160 sr-ME
  {ULScript_Latin, None, None, None, },  // 161 ak
  {ULScript_Latin, None, None, None, },  // 162 ig
  {ULScript_Latin, None, None, None, },  // 163 mfe
  {ULScript_Latin, None, None, None, },  // 164 haw
  {ULScript_Latin, None, None, None, },  // 165 ceb
  {ULScript_Latin, None, None, None, },  // 166 ee
  {ULScript_Latin, None, None, None, },  // 167 gaa
  {ULScript_Latin, None, None, None, },  // 168 hmn
  {ULScript_Latin, None, None, None, },  // 169 kri
  {ULScript_Latin, None, None, None, },  // 170 loz
  {ULScript_Latin, None, None, None, },  // 171 lua
  {ULScript_Latin, None, None, None, },  // 172 luo
  {ULScript_Devanagari, None, None, None, },  // 173 new
  {ULScript_Latin, None, None, None, },  // 174 ny
  {ULScript_Cyrillic, None, None, None, },  // 175 os
  {ULScript_Latin, None, None, None, },  // 176 pam
  {ULScript_Latin, None, None, None, },  // 177 nso
  {ULScript_Devanagari, None, None, None, },  // 178 raj
  {ULScript_Latin, None, None, None, },  // 179 crs
  {ULScript_Latin, None, None, None, },  // 180 tum
  {ULScript_Latin, None, None, None, },  // 181 ve
  {ULScript_Latin, None, None, None, },  // 182 war
  {None, None, None, None, },  // 183
  {None, None, None, None, },  // 184
  {None, None, None, None, },  // 185
  {None, None, None, None, },  // 186
  {None, None, None, None, },  // 187
  {None, None, None, None, },  // 188
  {None, None, None, None, },  // 189
  {None, None, None, None, },  // 190
  {None, None, None, None, },  // 191
  {None, None, None, None, },  // 192
  {None, None, None, None, },  // 193
  {None, None, None, None, },  // 194
  {None, None, None, None, },  // 195
  {None, None, None, None, },  // 196
  {None, None, None, None, },  // 197
  {None, None, None, None, },  // 198
  {None, None, None, None, },  // 199
  {None, None, None, None, },  // 200
  {None, None, None, None, },  // 201
  {None, None, None, None, },  // 202
  {None, None, None, None, },  // 203
  {None, None, None, None, },  // 204
  {None, None, None, None, },  // 205
  {None, None, None, None, },  // 206
  {None, None, None, None, },  // 207
  {None, None, None, None, },  // 208
  {None, None, None, None, },  // 209
  {None, None, None, None, },  // 210
  {None, None, None, None, },  // 211
  {None, None, None, None, },  // 212
  {None, None, None, None, },  // 213
  {None, None, None, None, },  // 214
  {None, None, None, None, },  // 215
  {None, None, None, None, },  // 216
  {None, None, None, None, },  // 217
  {None, None, None, None, },  // 218
  {None, None, None, None, },  // 219
  {None, None, None, None, },  // 220
  {None, None, None, None, },  // 221
  {None, None, None, None, },  // 222
  {None, None, None, None, },  // 223
  {None, None, None, None, },  // 224
  {None, None, None, None, },  // 225
  {None, None, None, None, },  // 226
  {None, None, None, None, },  // 227
  {None, None, None, None, },  // 228
  {None, None, None, None, },  // 229
  {None, None, None, None, },  // 230
  {None, None, None, None, },  // 231
  {None, None, None, None, },  // 232
  {None, None, None, None, },  // 233
  {None, None, None, None, },  // 234
  {None, None, None, None, },  // 235
  {None, None, None, None, },  // 236
  {None, None, None, None, },  // 237
  {None, None, None, None, },  // 238
  {None, None, None, None, },  // 239
  {None, None, None, None, },  // 240
  {None, None, None, None, },  // 241
  {None, None, None, None, },  // 242
  {None, None, None, None, },  // 243
  {None, None, None, None, },  // 244
  {None, None, None, None, },  // 245
  {None, None, None, None, },  // 246
  {None, None, None, None, },  // 247
  {None, None, None, None, },  // 248
  {None, None, None, None, },  // 249
  {None, None, None, None, },  // 250
  {None, None, None, None, },  // 251
  {None, None, None, None, },  // 252
  {None, None, None, None, },  // 253
  {None, None, None, None, },  // 254
  {None, None, None, None, },  // 255
  {None, None, None, None, },  // 256
  {None, None, None, None, },  // 257
  {None, None, None, None, },  // 258
  {None, None, None, None, },  // 259
  {None, None, None, None, },  // 260
  {None, None, None, None, },  // 261
  {None, None, None, None, },  // 262
  {None, None, None, None, },  // 263
  {None, None, None, None, },  // 264
  {None, None, None, None, },  // 265
  {None, None, None, None, },  // 266
  {None, None, None, None, },  // 267
  {None, None, None, None, },  // 268
  {None, None, None, None, },  // 269
  {None, None, None, None, },  // 270
  {None, None, None, None, },  // 271
  {None, None, None, None, },  // 272
  {None, None, None, None, },  // 273
  {None, None, None, None, },  // 274
  {None, None, None, None, },  // 275
  {None, None, None, None, },  // 276
  {None, None, None, None, },  // 277
  {None, None, None, None, },  // 278
  {None, None, None, None, },  // 279
  {None, None, None, None, },  // 280
  {None, None, None, None, },  // 281
  {None, None, None, None, },  // 282
  {None, None, None, None, },  // 283
  {None, None, None, None, },  // 284
  {None, None, None, None, },  // 285
  {None, None, None, None, },  // 286
  {None, None, None, None, },  // 287
  {None, None, None, None, },  // 288
  {None, None, None, None, },  // 289
  {None, None, None, None, },  // 290
  {None, None, None, None, },  // 291
  {None, None, None, None, },  // 292
  {None, None, None, None, },  // 293
  {None, None, None, None, },  // 294
  {None, None, None, None, },  // 295
  {None, None, None, None, },  // 296
  {None, None, None, None, },  // 297
  {None, None, None, None, },  // 298
  {None, None, None, None, },  // 299
  {None, None, None, None, },  // 300
  {None, None, None, None, },  // 301
  {None, None, None, None, },  // 302
  {None, None, None, None, },  // 303
  {None, None, None, None, },  // 304
  {None, None, None, None, },  // 305
  {None, None, None, None, },  // 306
  {None, None, None, None, },  // 307
  {None, None, None, None, },  // 308
  {None, None, None, None, },  // 309
  {None, None, None, None, },  // 310
  {None, None, None, None, },  // 311
  {None, None, None, None, },  // 312
  {None, None, None, None, },  // 313
  {None, None, None, None, },  // 314
  {None, None, None, None, },  // 315
  {None, None, None, None, },  // 316
  {None, None, None, None, },  // 317
  {None, None, None, None, },  // 318
  {None, None, None, None, },  // 319
  {None, None, None, None, },  // 320
  {None, None, None, None, },  // 321
  {None, None, None, None, },  // 322
  {None, None, None, None, },  // 323
  {None, None, None, None, },  // 324
  {None, None, None, None, },  // 325
  {None, None, None, None, },  // 326
  {None, None, None, None, },  // 327
  {None, None, None, None, },  // 328
  {None, None, None, None, },  // 329
  {None, None, None, None, },  // 330
  {None, None, None, None, },  // 331
  {None, None, None, None, },  // 332
  {None, None, None, None, },  // 333
  {None, None, None, None, },  // 334
  {None, None, None, None, },  // 335
  {None, None, None, None, },  // 336
  {None, None, None, None, },  // 337
  {None, None, None, None, },  // 338
  {None, None, None, None, },  // 339
  {None, None, None, None, },  // 340
  {None, None, None, None, },  // 341
  {None, None, None, None, },  // 342
  {None, None, None, None, },  // 343
  {None, None, None, None, },  // 344
  {None, None, None, None, },  // 345
  {None, None, None, None, },  // 346
  {None, None, None, None, },  // 347
  {None, None, None, None, },  // 348
  {None, None, None, None, },  // 349
  {None, None, None, None, },  // 350
  {None, None, None, None, },  // 351
  {None, None, None, None, },  // 352
  {None, None, None, None, },  // 353
  {None, None, None, None, },  // 354
  {None, None, None, None, },  // 355
  {None, None, None, None, },  // 356
  {None, None, None, None, },  // 357
  {None, None, None, None, },  // 358
  {None, None, None, None, },  // 359
  {None, None, None, None, },  // 360
  {None, None, None, None, },  // 361
  {None, None, None, None, },  // 362
  {None, None, None, None, },  // 363
  {None, None, None, None, },  // 364
  {None, None, None, None, },  // 365
  {None, None, None, None, },  // 366
  {None, None, None, None, },  // 367
  {None, None, None, None, },  // 368
  {None, None, None, None, },  // 369
  {None, None, None, None, },  // 370
  {None, None, None, None, },  // 371
  {None, None, None, None, },  // 372
  {None, None, None, None, },  // 373
  {None, None, None, None, },  // 374
  {None, None, None, None, },  // 375
  {None, None, None, None, },  // 376
  {None, None, None, None, },  // 377
  {None, None, None, None, },  // 378
  {None, None, None, None, },  // 379
  {None, None, None, None, },  // 380
  {None, None, None, None, },  // 381
  {None, None, None, None, },  // 382
  {None, None, None, None, },  // 383
  {None, None, None, None, },  // 384
  {None, None, None, None, },  // 385
  {None, None, None, None, },  // 386
  {None, None, None, None, },  // 387
  {None, None, None, None, },  // 388
  {None, None, None, None, },  // 389
  {None, None, None, None, },  // 390
  {None, None, None, None, },  // 391
  {None, None, None, None, },  // 392
  {None, None, None, None, },  // 393
  {None, None, None, None, },  // 394
  {None, None, None, None, },  // 395
  {None, None, None, None, },  // 396
  {None, None, None, None, },  // 397
  {None, None, None, None, },  // 398
  {None, None, None, None, },  // 399
  {None, None, None, None, },  // 400
  {None, None, None, None, },  // 401
  {None, None, None, None, },  // 402
  {None, None, None, None, },  // 403
  {None, None, None, None, },  // 404
  {None, None, None, None, },  // 405
  {None, None, None, None, },  // 406
  {None, None, None, None, },  // 407
  {None, None, None, None, },  // 408
  {None, None, None, None, },  // 409
  {None, None, None, None, },  // 410
  {None, None, None, None, },  // 411
  {None, None, None, None, },  // 412
  {None, None, None, None, },  // 413
  {None, None, None, None, },  // 414
  {None, None, None, None, },  // 415
  {None, None, None, None, },  // 416
  {None, None, None, None, },  // 417
  {None, None, None, None, },  // 418
  {None, None, None, None, },  // 419
  {None, None, None, None, },  // 420
  {None, None, None, None, },  // 421
  {None, None, None, None, },  // 422
  {None, None, None, None, },  // 423
  {None, None, None, None, },  // 424
  {None, None, None, None, },  // 425
  {None, None, None, None, },  // 426
  {None, None, None, None, },  // 427
  {None, None, None, None, },  // 428
  {None, None, None, None, },  // 429
  {None, None, None, None, },  // 430
  {None, None, None, None, },  // 431
  {None, None, None, None, },  // 432
  {None, None, None, None, },  // 433
  {None, None, None, None, },  // 434
  {None, None, None, None, },  // 435
  {None, None, None, None, },  // 436
  {None, None, None, None, },  // 437
  {None, None, None, None, },  // 438
  {None, None, None, None, },  // 439
  {None, None, None, None, },  // 440
  {None, None, None, None, },  // 441
  {None, None, None, None, },  // 442
  {None, None, None, None, },  // 443
  {None, None, None, None, },  // 444
  {None, None, None, None, },  // 445
  {None, None, None, None, },  // 446
  {None, None, None, None, },  // 447
  {None, None, None, None, },  // 448
  {None, None, None, None, },  // 449
  {None, None, None, None, },  // 450
  {None, None, None, None, },  // 451
  {None, None, None, None, },  // 452
  {None, None, None, None, },  // 453
  {None, None, None, None, },  // 454
  {None, None, None, None, },  // 455
  {None, None, None, None, },  // 456
  {None, None, None, None, },  // 457
  {None, None, None, None, },  // 458
  {None, None, None, None, },  // 459
  {None, None, None, None, },  // 460
  {None, None, None, None, },  // 461
  {None, None, None, None, },  // 462
  {None, None, None, None, },  // 463
  {None, None, None, None, },  // 464
  {None, None, None, None, },  // 465
  {None, None, None, None, },  // 466
  {None, None, None, None, },  // 467
  {None, None, None, None, },  // 468
  {None, None, None, None, },  // 469
  {None, None, None, None, },  // 470
  {None, None, None, None, },  // 471
  {None, None, None, None, },  // 472
  {None, None, None, None, },  // 473
  {None, None, None, None, },  // 474
  {None, None, None, None, },  // 475
  {None, None, None, None, },  // 476
  {None, None, None, None, },  // 477
  {None, None, None, None, },  // 478
  {None, None, None, None, },  // 479
  {None, None, None, None, },  // 480
  {None, None, None, None, },  // 481
  {None, None, None, None, },  // 482
  {None, None, None, None, },  // 483
  {None, None, None, None, },  // 484
  {None, None, None, None, },  // 485
  {None, None, None, None, },  // 486
  {None, None, None, None, },  // 487
  {None, None, None, None, },  // 488
  {None, None, None, None, },  // 489
  {None, None, None, None, },  // 490
  {None, None, None, None, },  // 491
  {None, None, None, None, },  // 492
  {None, None, None, None, },  // 493
  {None, None, None, None, },  // 494
  {None, None, None, None, },  // 495
  {None, None, None, None, },  // 496
  {None, None, None, None, },  // 497
  {None, None, None, None, },  // 498
  {None, None, None, None, },  // 499
  {None, None, None, None, },  // 500
  {None, None, None, None, },  // 501
  {None, None, None, None, },  // 502
  {None, None, None, None, },  // 503
  {None, None, None, None, },  // 504
  {None, None, None, None, },  // 505
  {ULScript_Latin, None, None, None, },  // 506 nr
  {ULScript_Latin, None, None, None, },  // 507 zzb
  {ULScript_Latin, None, None, None, },  // 508 zzp
  {ULScript_Latin, None, None, None, },  // 509 zzh
  {ULScript_Latin, None, None, None, },  // 510 tlh
  {ULScript_Latin, None, None, None, },  // 511 zze
  {None, None, None, None, },  // 512 xx-Zyyy
  {ULScript_Latin, None, None, None, },  // 513 xx-Latn
  {ULScript_Greek, None, None, None, },  // 514 xx-Grek
  {ULScript_Cyrillic, None, None, None, },  // 515 xx-Cyrl
  {ULScript_Armenian, None, None, None, },  // 516 xx-Armn
  {ULScript_Hebrew, None, None, None, },  // 517 xx-Hebr
  {ULScript_Arabic, None, None, None, },  // 518 xx-Arab
  {ULScript_Syriac, None, None, None, },  // 519 xx-Syrc
  {ULScript_Thaana, None, None, None, },  // 520 xx-Thaa
  {ULScript_Devanagari, None, None, None, },  // 521 xx-Deva
  {ULScript_Bengali, None, None, None, },  // 522 xx-Beng
  {ULScript_Gurmukhi, None, None, None, },  // 523 xx-Guru
  {ULScript_Gujarati, None, None, None, },  // 524 xx-Gujr
  {ULScript_Oriya, None, None, None, },  // 525 xx-Orya
  {ULScript_Tamil, None, None, None, },  // 526 xx-Taml
  {ULScript_Telugu, None, None, None, },  // 527 xx-Telu
  {ULScript_Kannada, None, None, None, },  // 528 xx-Knda
  {ULScript_Malayalam, None, None, None, },  // 529 xx-Mlym
  {ULScript_Sinhala, None, None, None, },  // 530 xx-Sinh
  {ULScript_Thai, None, None, None, },  // 531 xx-Thai
  {ULScript_Lao, None, None, None, },  // 532 xx-Laoo
  {ULScript_Tibetan, None, None, None, },  // 533 xx-Tibt
  {ULScript_Myanmar, None, None, None, },  // 534 xx-Mymr
  {ULScript_Georgian, None, None, None, },  // 535 xx-Geor
  {None, None, None, None, },  // 536 xx-Hang
  {ULScript_Ethiopic, None, None, None, },  // 537 xx-Ethi
  {ULScript_Cherokee, None, None, None, },  // 538 xx-Cher
  {ULScript_Canadian_Aboriginal, None, None, None, },  // 539 xx-Cans
  {ULScript_Ogham, None, None, None, },  // 540 xx-Ogam
  {ULScript_Runic, None, None, None, },  // 541 xx-Runr
  {ULScript_Khmer, None, None, None, },  // 542 xx-Khmr
  {ULScript_Mongolian, None, None, None, },  // 543 xx-Mong
  {None, None, None, None, },  // 544 xx-Hira
  {None, None, None, None, },  // 545 xx-Kana
  {ULScript_Bopomofo, None, None, None, },  // 546 xx-Bopo
  {ULScript_Hani, None, None, None, },  // 547 xx-Hani
  {ULScript_Yi, None, None, None, },  // 548 xx-Yiii
  {ULScript_Old_Italic, None, None, None, },  // 549 xx-Ital
  {ULScript_Gothic, None, None, None, },  // 550 xx-Goth
  {ULScript_Deseret, None, None, None, },  // 551 xx-Dsrt
  {None, None, None, None, },  // 552 xx-Qaai
  {ULScript_Tagalog, None, None, None, },  // 553 xx-Tglg
  {ULScript_Hanunoo, None, None, None, },  // 554 xx-Hano
  {ULScript_Buhid, None, None, None, },  // 555 xx-Buhd
  {ULScript_Tagbanwa, None, None, None, },  // 556 xx-Tagb
  {ULScript_Limbu, None, None, None, },  // 557 xx-Limb
  {ULScript_Tai_Le, None, None, None, },  // 558 xx-Tale
  {ULScript_Linear_B, None, None, None, },  // 559 xx-Linb
  {ULScript_Ugaritic, None, None, None, },  // 560 xx-Ugar
  {ULScript_Shavian, None, None, None, },  // 561 xx-Shaw
  {ULScript_Osmanya, None, None, None, },  // 562 xx-Osma
  {ULScript_Cypriot, None, None, None, },  // 563 xx-Cprt
  {ULScript_Braille, None, None, None, },  // 564 xx-Brai
  {ULScript_Buginese, None, None, None, },  // 565 xx-Bugi
  {ULScript_Coptic, None, None, None, },  // 566 xx-Copt
  {ULScript_New_Tai_Lue, None, None, None, },  // 567 xx-Talu
  {ULScript_Glagolitic, None, None, None, },  // 568 xx-Glag
  {ULScript_Tifinagh, None, None, None, },  // 569 xx-Tfng
  {ULScript_Syloti_Nagri, None, None, None, },  // 570 xx-Sylo
  {ULScript_Old_Persian, None, None, None, },  // 571 xx-Xpeo
  {ULScript_Kharoshthi, None, None, None, },  // 572 xx-Khar
  {ULScript_Balinese, None, None, None, },  // 573 xx-Bali
  {ULScript_Cuneiform, None, None, None, },  // 574 xx-Xsux
  {ULScript_Phoenician, None, None, None, },  // 575 xx-Phnx
  {ULScript_Phags_Pa, None, None, None, },  // 576 xx-Phag
  {ULScript_Nko, None, None, None, },  // 577 xx-Nkoo
  {ULScript_Sundanese, None, None, None, },  // 578 xx-Sund
  {ULScript_Lepcha, None, None, None, },  // 579 xx-Lepc
  {ULScript_Ol_Chiki, None, None, None, },  // 580 xx-Olck
  {ULScript_Vai, None, None, None, },  // 581 xx-Vaii
  {ULScript_Saurashtra, None, None, None, },  // 582 xx-Saur
  {ULScript_Kayah_Li, None, None, None, },  // 583 xx-Kali
  {ULScript_Rejang, None, None, None, },  // 584 xx-Rjng
  {ULScript_Lycian, None, None, None, },  // 585 xx-Lyci
  {ULScript_Carian, None, None, None, },  // 586 xx-Cari
  {ULScript_Lydian, None, None, None, },  // 587 xx-Lydi
  {ULScript_Cham, None, None, None, },  // 588 xx-Cham
  {ULScript_Tai_Tham, None, None, None, },  // 589 xx-Lana
  {ULScript_Tai_Viet, None, None, None, },  // 590 xx-Tavt
  {ULScript_Avestan, None, None, None, },  // 591 xx-Avst
  {ULScript_Egyptian_Hieroglyphs, None, None, None, },  // 592 xx-Egyp
  {ULScript_Samaritan, None, None, None, },  // 593 xx-Samr
  {ULScript_Lisu, None, None, None, },  // 594 xx-Lisu
  {ULScript_Bamum, None, None, None, },  // 595 xx-Bamu
  {ULScript_Javanese, None, None, None, },  // 596 xx-Java
  {ULScript_Meetei_Mayek, None, None, None, },  // 597 xx-Mtei
  {ULScript_Imperial_Aramaic, None, None, None, },  // 598 xx-Armi
  {ULScript_Old_South_Arabian, None, None, None, },  // 599 xx-Sarb
  {ULScript_Inscriptional_Parthian, None, None, None, },  // 600 xx-Prti
  {ULScript_Inscriptional_Pahlavi, None, None, None, },  // 601 xx-Phli
  {ULScript_Old_Turkic, None, None, None, },  // 602 xx-Orkh
  {ULScript_Kaithi, None, None, None, },  // 603 xx-Kthi
  {ULScript_Batak, None, None, None, },  // 604 xx-Batk
  {ULScript_Brahmi, None, None, None, },  // 605 xx-Brah
  {ULScript_Mandaic, None, None, None, },  // 606 xx-Mand
  {ULScript_Chakma, None, None, None, },  // 607 xx-Cakm
  {ULScript_Meroitic_Cursive, None, None, None, },  // 608 xx-Merc
  {ULScript_Meroitic_Hieroglyphs, None, None, None, },  // 609 xx-Mero
  {ULScript_Miao, None, None, None, },  // 610 xx-Plrd
  {ULScript_Sharada, None, None, None, },  // 611 xx-Shrd
  {ULScript_Sora_Sompeng, None, None, None, },  // 612 xx-Sora
  {ULScript_Takri, None, None, None, },  // 613 xx-Takr
};
#undef None

// Subscripted by enum Language
extern const int kLanguageToPLangSize = 512;
extern const uint8 kLanguageToPLang[kLanguageToPLangSize] = {
    1,  // 0 en
    2,  // 1 da
    3,  // 2 nl
    4,  // 3 fi
    5,  // 4 fr
    6,  // 5 de
    1,  // 6 iw
    7,  // 7 it
    2,  // 8 ja
    3,  // 9 ko
    8,  // 10 no
    9,  // 11 pl
   10,  // 12 pt
    4,  // 13 ru
   11,  // 14 es
   12,  // 15 sv
    5,  // 16 zh
   13,  // 17 cs
    6,  // 18 el
   14,  // 19 is
   15,  // 20 lv
   16,  // 21 lt
   17,  // 22 ro
   18,  // 23 hu
   19,  // 24 et
   20,  // 25 xxx
   21,  // 26 un
    7,  // 27 bg
   22,  // 28 hr
   23,  // 29 sr
   24,  // 30 ga
   25,  // 31 gl
   26,  // 32 tl
   27,  // 33 tr
    8,  // 34 uk
    9,  // 35 hi
   10,  // 36 mk
   11,  // 37 bn
   28,  // 38 id
   29,  // 39 la
   30,  // 40 ms
   12,  // 41 ml
   31,  // 42 cy
   13,  // 43 ne
   14,  // 44 te
   32,  // 45 sq
   15,  // 46 ta
   16,  // 47 be
   33,  // 48 jw
   34,  // 49 oc
   18,  // 50 ur
   19,  // 51 bh
   21,  // 52 gu
   22,  // 53 th
   24,  // 54 ar
   35,  // 55 ca
   36,  // 56 eo
   37,  // 57 eu
   38,  // 58 ia
   25,  // 59 kn
   27,  // 60 pa
   39,  // 61 gd
   40,  // 62 sw
   41,  // 63 sl
   28,  // 64 mr
   42,  // 65 mt
   43,  // 66 vi
   44,  // 67 fy
   45,  // 68 sk
   29,  // 69 zh-Hant
   46,  // 70 fo
   47,  // 71 su
   48,  // 72 uz
   30,  // 73 am
   49,  // 74 az
   31,  // 75 ka
   32,  // 76 ti
   33,  // 77 fa
   50,  // 78 bs
   34,  // 79 si
   51,  // 80 nn
    0,  // 81
    0,  // 82
   52,  // 83 xh
   53,  // 84 zu
   54,  // 85 gn
   55,  // 86 st
   56,  // 87 tk
   35,  // 88 ky
   57,  // 89 br
   58,  // 90 tw
   36,  // 91 yi
    0,  // 92
   59,  // 93 so
   60,  // 94 ug
   61,  // 95 ku
   37,  // 96 mn
   38,  // 97 hy
   39,  // 98 lo
   40,  // 99 sd
   62,  // 100 rm
   63,  // 101 af
   64,  // 102 lb
   65,  // 103 my
   41,  // 104 km
   42,  // 105 bo
   43,  // 106 dv
   44,  // 107 chr
   45,  // 108 syr
   46,  // 109 lif
   47,  // 110 or
   51,  // 111 as
   66,  // 112 co
   67,  // 113 ie
   68,  // 114 kk
   69,  // 115 ln
    0,  // 116
   52,  // 117 ps
   70,  // 118 qu
   71,  // 119 sn
   53,  // 120 tg
   72,  // 121 tt
   73,  // 122 to
   74,  // 123 yo
    0,  // 124
    0,  // 125
    0,  // 126
    0,  // 127
   75,  // 128 mi
   76,  // 129 wo
   54,  // 130 ab
   77,  // 131 aa
   78,  // 132 ay
   55,  // 133 ba
   79,  // 134 bi
   57,  // 135 dz
   80,  // 136 fj
   81,  // 137 kl
   82,  // 138 ha
   83,  // 139 ht
   84,  // 140 ik
   58,  // 141 iu
   59,  // 142 ks
   85,  // 143 rw
   86,  // 144 mg
   87,  // 145 na
   88,  // 146 om
   89,  // 147 rn
   90,  // 148 sm
   91,  // 149 sg
   92,  // 150 sa
   93,  // 151 ss
   94,  // 152 ts
   95,  // 153 tn
   96,  // 154 vo
   97,  // 155 za
   98,  // 156 kha
   99,  // 157 sco
  100,  // 158 lg
  101,  // 159 gv
  102,  // 160 sr-ME
  103,  // 161 ak
  104,  // 162 ig
  105,  // 163 mfe
  106,  // 164 haw
  107,  // 165 ceb
  108,  // 166 ee
  109,  // 167 gaa
  110,  // 168 hmn
  111,  // 169 kri
  112,  // 170 loz
  113,  // 171 lua
  114,  // 172 luo
   62,  // 173 new
  115,  // 174 ny
   63,  // 175 os
  116,  // 176 pam
  117,  // 177 nso
   64,  // 178 raj
  118,  // 179 crs
  119,  // 180 tum
  120,  // 181 ve
  121,  // 182 war
    0,  // 183
    0,  // 184
    0,  // 185
    0,  // 186
    0,  // 187
    0,  // 188
    0,  // 189
    0,  // 190
    0,  // 191
    0,  // 192
    0,  // 193
    0,  // 194
    0,  // 195
    0,  // 196
    0,  // 197
    0,  // 198
    0,  // 199
    0,  // 200
    0,  // 201
    0,  // 202
    0,  // 203
    0,  // 204
    0,  // 205
    0,  // 206
    0,  // 207
    0,  // 208
    0,  // 209
    0,  // 210
    0,  // 211
    0,  // 212
    0,  // 213
    0,  // 214
    0,  // 215
    0,  // 216
    0,  // 217
    0,  // 218
    0,  // 219
    0,  // 220
    0,  // 221
    0,  // 222
    0,  // 223
    0,  // 224
    0,  // 225
    0,  // 226
    0,  // 227
    0,  // 228
    0,  // 229
    0,  // 230
    0,  // 231
    0,  // 232
    0,  // 233
    0,  // 234
    0,  // 235
    0,  // 236
    0,  // 237
    0,  // 238
    0,  // 239
    0,  // 240
    0,  // 241
    0,  // 242
    0,  // 243
    0,  // 244
    0,  // 245
    0,  // 246
    0,  // 247
    0,  // 248
    0,  // 249
    0,  // 250
    0,  // 251
    0,  // 252
    0,  // 253
    0,  // 254
    0,  // 255
    0,  // 256
    0,  // 257
    0,  // 258
    0,  // 259
    0,  // 260
    0,  // 261
    0,  // 262
    0,  // 263
    0,  // 264
    0,  // 265
    0,  // 266
    0,  // 267
    0,  // 268
    0,  // 269
    0,  // 270
    0,  // 271
    0,  // 272
    0,  // 273
    0,  // 274
    0,  // 275
    0,  // 276
    0,  // 277
    0,  // 278
    0,  // 279
    0,  // 280
    0,  // 281
    0,  // 282
    0,  // 283
    0,  // 284
    0,  // 285
    0,  // 286
    0,  // 287
    0,  // 288
    0,  // 289
    0,  // 290
    0,  // 291
    0,  // 292
    0,  // 293
    0,  // 294
    0,  // 295
    0,  // 296
    0,  // 297
    0,  // 298
    0,  // 299
    0,  // 300
    0,  // 301
    0,  // 302
    0,  // 303
    0,  // 304
    0,  // 305
    0,  // 306
    0,  // 307
    0,  // 308
    0,  // 309
    0,  // 310
    0,  // 311
    0,  // 312
    0,  // 313
    0,  // 314
    0,  // 315
    0,  // 316
    0,  // 317
    0,  // 318
    0,  // 319
    0,  // 320
    0,  // 321
    0,  // 322
    0,  // 323
    0,  // 324
    0,  // 325
    0,  // 326
    0,  // 327
    0,  // 328
    0,  // 329
    0,  // 330
    0,  // 331
    0,  // 332
    0,  // 333
    0,  // 334
    0,  // 335
    0,  // 336
    0,  // 337
    0,  // 338
    0,  // 339
    0,  // 340
    0,  // 341
    0,  // 342
    0,  // 343
    0,  // 344
    0,  // 345
    0,  // 346
    0,  // 347
    0,  // 348
    0,  // 349
    0,  // 350
    0,  // 351
    0,  // 352
    0,  // 353
    0,  // 354
    0,  // 355
    0,  // 356
    0,  // 357
    0,  // 358
    0,  // 359
    0,  // 360
    0,  // 361
    0,  // 362
    0,  // 363
    0,  // 364
    0,  // 365
    0,  // 366
    0,  // 367
    0,  // 368
    0,  // 369
    0,  // 370
    0,  // 371
    0,  // 372
    0,  // 373
    0,  // 374
    0,  // 375
    0,  // 376
    0,  // 377
    0,  // 378
    0,  // 379
    0,  // 380
    0,  // 381
    0,  // 382
    0,  // 383
    0,  // 384
    0,  // 385
    0,  // 386
    0,  // 387
    0,  // 388
    0,  // 389
    0,  // 390
    0,  // 391
    0,  // 392
    0,  // 393
    0,  // 394
    0,  // 395
    0,  // 396
    0,  // 397
    0,  // 398
    0,  // 399
    0,  // 400
    0,  // 401
    0,  // 402
    0,  // 403
    0,  // 404
    0,  // 405
    0,  // 406
    0,  // 407
    0,  // 408
    0,  // 409
    0,  // 410
    0,  // 411
    0,  // 412
    0,  // 413
    0,  // 414
    0,  // 415
    0,  // 416
    0,  // 417
    0,  // 418
    0,  // 419
    0,  // 420
    0,  // 421
    0,  // 422
    0,  // 423
    0,  // 424
    0,  // 425
    0,  // 426
    0,  // 427
    0,  // 428
    0,  // 429
    0,  // 430
    0,  // 431
    0,  // 432
    0,  // 433
    0,  // 434
    0,  // 435
    0,  // 436
    0,  // 437
    0,  // 438
    0,  // 439
    0,  // 440
    0,  // 441
    0,  // 442
    0,  // 443
    0,  // 444
    0,  // 445
    0,  // 446
    0,  // 447
    0,  // 448
    0,  // 449
    0,  // 450
    0,  // 451
    0,  // 452
    0,  // 453
    0,  // 454
    0,  // 455
    0,  // 456
    0,  // 457
    0,  // 458
    0,  // 459
    0,  // 460
    0,  // 461
    0,  // 462
    0,  // 463
    0,  // 464
    0,  // 465
    0,  // 466
    0,  // 467
    0,  // 468
    0,  // 469
    0,  // 470
    0,  // 471
    0,  // 472
    0,  // 473
    0,  // 474
    0,  // 475
    0,  // 476
    0,  // 477
    0,  // 478
    0,  // 479
    0,  // 480
    0,  // 481
    0,  // 482
    0,  // 483
    0,  // 484
    0,  // 485
    0,  // 486
    0,  // 487
    0,  // 488
    0,  // 489
    0,  // 490
    0,  // 491
    0,  // 492
    0,  // 493
    0,  // 494
    0,  // 495
    0,  // 496
    0,  // 497
    0,  // 498
    0,  // 499
    0,  // 500
    0,  // 501
    0,  // 502
    0,  // 503
    0,  // 504
    0,  // 505
  250,  // 506 nr
  251,  // 507 zzb
  252,  // 508 zzp
  253,  // 509 zzh
  254,  // 510 tlh
  255,  // 511 zze
};

// Subscripted by PLang, for ULScript = Latn
extern const uint16 kPLangToLanguageLatn[256] = {
  UNKNOWN_LANGUAGE,      // 0
  ENGLISH,               // 1
  DANISH,                // 2
  DUTCH,                 // 3
  FINNISH,               // 4
  FRENCH,                // 5
  GERMAN,                // 6
  ITALIAN,               // 7
  NORWEGIAN,             // 8
  POLISH,                // 9
  PORTUGUESE,            // 10
  SPANISH,               // 11
  SWEDISH,               // 12
  CZECH,                 // 13
  ICELANDIC,             // 14
  LATVIAN,               // 15
  LITHUANIAN,            // 16
  ROMANIAN,              // 17
  HUNGARIAN,             // 18
  ESTONIAN,              // 19
  TG_UNKNOWN_LANGUAGE,   // 20
  UNKNOWN_LANGUAGE,      // 21
  CROATIAN,              // 22
  SERBIAN,               // 23
  IRISH,                 // 24
  GALICIAN,              // 25
  TAGALOG,               // 26
  TURKISH,               // 27
  INDONESIAN,            // 28
  LATIN,                 // 29
  MALAY,                 // 30
  WELSH,                 // 31
  ALBANIAN,              // 32
  JAVANESE,              // 33
  OCCITAN,               // 34
  CATALAN,               // 35
  ESPERANTO,             // 36
  BASQUE,                // 37
  INTERLINGUA,           // 38
  SCOTS_GAELIC,          // 39
  SWAHILI,               // 40
  SLOVENIAN,             // 41
  MALTESE,               // 42
  VIETNAMESE,            // 43
  FRISIAN,               // 44
  SLOVAK,                // 45
  FAROESE,               // 46
  SUNDANESE,             // 47
  UZBEK,                 // 48
  AZERBAIJANI,           // 49
  BOSNIAN,               // 50
  NORWEGIAN_N,           // 51
  XHOSA,                 // 52
  ZULU,                  // 53
  GUARANI,               // 54
  SESOTHO,               // 55
  TURKMEN,               // 56
  BRETON,                // 57
  TWI,                   // 58
  SOMALI,                // 59
  UIGHUR,                // 60
  KURDISH,               // 61
  RHAETO_ROMANCE,        // 62
  AFRIKAANS,             // 63
  LUXEMBOURGISH,         // 64
  BURMESE,               // 65
  CORSICAN,              // 66
  INTERLINGUE,           // 67
  KAZAKH,                // 68
  LINGALA,               // 69
  QUECHUA,               // 70
  SHONA,                 // 71
  TATAR,                 // 72
  TONGA,                 // 73
  YORUBA,                // 74
  MAORI,                 // 75
  WOLOF,                 // 76
  AFAR,                  // 77
  AYMARA,                // 78
  BISLAMA,               // 79
  FIJIAN,                // 80
  GREENLANDIC,           // 81
  HAUSA,                 // 82
  HAITIAN_CREOLE,        // 83
  INUPIAK,               // 84
  KINYARWANDA,           // 85
  MALAGASY,              // 86
  NAURU,                 // 87
  OROMO,                 // 88
  RUNDI,                 // 89
  SAMOAN,                // 90
  SANGO,                 // 91
  SANSKRIT,              // 92
  SISWANT,               // 93
  TSONGA,                // 94
  TSWANA,                // 95
  VOLAPUK,               // 96
  ZHUANG,                // 97
  KHASI,                 // 98
  SCOTS,                 // 99
  GANDA,                 // 100
  MANX,                  // 101
  MONTENEGRIN,           // 102
  AKAN,                  // 103
  IGBO,                  // 104
  MAURITIAN_CREOLE,      // 105
  HAWAIIAN,              // 106
  CEBUANO,               // 107
  EWE,                   // 108
  GA,                    // 109
  HMONG,                 // 110
  KRIO,                  // 111
  LOZI,                  // 112
  LUBA_LULUA,            // 113
  LUO_KENYA_AND_TANZANIA,  // 114
  NYANJA,                // 115
  PAMPANGA,              // 116
  PEDI,                  // 117
  SESELWA,               // 118
  TUMBUKA,               // 119
  VENDA,                 // 120
  WARAY_PHILIPPINES,     // 121
  UNKNOWN_LANGUAGE,      // 122
  UNKNOWN_LANGUAGE,      // 123
  UNKNOWN_LANGUAGE,      // 124
  UNKNOWN_LANGUAGE,      // 125
  UNKNOWN_LANGUAGE,      // 126
  UNKNOWN_LANGUAGE,      // 127
  UNKNOWN_LANGUAGE,      // 128
  UNKNOWN_LANGUAGE,      // 129
  UNKNOWN_LANGUAGE,      // 130
  UNKNOWN_LANGUAGE,      // 131
  UNKNOWN_LANGUAGE,      // 132
  UNKNOWN_LANGUAGE,      // 133
  UNKNOWN_LANGUAGE,      // 134
  UNKNOWN_LANGUAGE,      // 135
  UNKNOWN_LANGUAGE,      // 136
  UNKNOWN_LANGUAGE,      // 137
  UNKNOWN_LANGUAGE,      // 138
  UNKNOWN_LANGUAGE,      // 139
  UNKNOWN_LANGUAGE,      // 140
  UNKNOWN_LANGUAGE,      // 141
  UNKNOWN_LANGUAGE,      // 142
  UNKNOWN_LANGUAGE,      // 143
  UNKNOWN_LANGUAGE,      // 144
  UNKNOWN_LANGUAGE,      // 145
  UNKNOWN_LANGUAGE,      // 146
  UNKNOWN_LANGUAGE,      // 147
  UNKNOWN_LANGUAGE,      // 148
  UNKNOWN_LANGUAGE,      // 149
  UNKNOWN_LANGUAGE,      // 150
  UNKNOWN_LANGUAGE,      // 151
  UNKNOWN_LANGUAGE,      // 152
  UNKNOWN_LANGUAGE,      // 153
  UNKNOWN_LANGUAGE,      // 154
  UNKNOWN_LANGUAGE,      // 155
  UNKNOWN_LANGUAGE,      // 156
  UNKNOWN_LANGUAGE,      // 157
  UNKNOWN_LANGUAGE,      // 158
  UNKNOWN_LANGUAGE,      // 159
  UNKNOWN_LANGUAGE,      // 160
  UNKNOWN_LANGUAGE,      // 161
  UNKNOWN_LANGUAGE,      // 162
  UNKNOWN_LANGUAGE,      // 163
  UNKNOWN_LANGUAGE,      // 164
  UNKNOWN_LANGUAGE,      // 165
  UNKNOWN_LANGUAGE,      // 166
  UNKNOWN_LANGUAGE,      // 167
  UNKNOWN_LANGUAGE,      // 168
  UNKNOWN_LANGUAGE,      // 169
  UNKNOWN_LANGUAGE,      // 170
  UNKNOWN_LANGUAGE,      // 171
  UNKNOWN_LANGUAGE,      // 172
  UNKNOWN_LANGUAGE,      // 173
  UNKNOWN_LANGUAGE,      // 174
  UNKNOWN_LANGUAGE,      // 175
  UNKNOWN_LANGUAGE,      // 176
  UNKNOWN_LANGUAGE,      // 177
  UNKNOWN_LANGUAGE,      // 178
  UNKNOWN_LANGUAGE,      // 179
  UNKNOWN_LANGUAGE,      // 180
  UNKNOWN_LANGUAGE,      // 181
  UNKNOWN_LANGUAGE,      // 182
  UNKNOWN_LANGUAGE,      // 183
  UNKNOWN_LANGUAGE,      // 184
  UNKNOWN_LANGUAGE,      // 185
  UNKNOWN_LANGUAGE,      // 186
  UNKNOWN_LANGUAGE,      // 187
  UNKNOWN_LANGUAGE,      // 188
  UNKNOWN_LANGUAGE,      // 189
  UNKNOWN_LANGUAGE,      // 190
  UNKNOWN_LANGUAGE,      // 191
  UNKNOWN_LANGUAGE,      // 192
  UNKNOWN_LANGUAGE,      // 193
  UNKNOWN_LANGUAGE,      // 194
  UNKNOWN_LANGUAGE,      // 195
  UNKNOWN_LANGUAGE,      // 196
  UNKNOWN_LANGUAGE,      // 197
  UNKNOWN_LANGUAGE,      // 198
  UNKNOWN_LANGUAGE,      // 199
  UNKNOWN_LANGUAGE,      // 200
  UNKNOWN_LANGUAGE,      // 201
  UNKNOWN_LANGUAGE,      // 202
  UNKNOWN_LANGUAGE,      // 203
  UNKNOWN_LANGUAGE,      // 204
  UNKNOWN_LANGUAGE,      // 205
  UNKNOWN_LANGUAGE,      // 206
  UNKNOWN_LANGUAGE,      // 207
  UNKNOWN_LANGUAGE,      // 208
  UNKNOWN_LANGUAGE,      // 209
  UNKNOWN_LANGUAGE,      // 210
  UNKNOWN_LANGUAGE,      // 211
  UNKNOWN_LANGUAGE,      // 212
  UNKNOWN_LANGUAGE,      // 213
  UNKNOWN_LANGUAGE,      // 214
  UNKNOWN_LANGUAGE,      // 215
  UNKNOWN_LANGUAGE,      // 216
  UNKNOWN_LANGUAGE,      // 217
  UNKNOWN_LANGUAGE,      // 218
  UNKNOWN_LANGUAGE,      // 219
  UNKNOWN_LANGUAGE,      // 220
  UNKNOWN_LANGUAGE,      // 221
  UNKNOWN_LANGUAGE,      // 222
  UNKNOWN_LANGUAGE,      // 223
  UNKNOWN_LANGUAGE,      // 224
  UNKNOWN_LANGUAGE,      // 225
  UNKNOWN_LANGUAGE,      // 226
  UNKNOWN_LANGUAGE,      // 227
  UNKNOWN_LANGUAGE,      // 228
  UNKNOWN_LANGUAGE,      // 229
  UNKNOWN_LANGUAGE,      // 230
  UNKNOWN_LANGUAGE,      // 231
  UNKNOWN_LANGUAGE,      // 232
  UNKNOWN_LANGUAGE,      // 233
  UNKNOWN_LANGUAGE,      // 234
  UNKNOWN_LANGUAGE,      // 235
  UNKNOWN_LANGUAGE,      // 236
  UNKNOWN_LANGUAGE,      // 237
  UNKNOWN_LANGUAGE,      // 238
  UNKNOWN_LANGUAGE,      // 239
  UNKNOWN_LANGUAGE,      // 240
  UNKNOWN_LANGUAGE,      // 241
  UNKNOWN_LANGUAGE,      // 242
  UNKNOWN_LANGUAGE,      // 243
  UNKNOWN_LANGUAGE,      // 244
  UNKNOWN_LANGUAGE,      // 245
  UNKNOWN_LANGUAGE,      // 246
  UNKNOWN_LANGUAGE,      // 247
  UNKNOWN_LANGUAGE,      // 248
  UNKNOWN_LANGUAGE,      // 249
  NDEBELE,               // 250
  X_BORK_BORK_BORK,      // 251
  X_PIG_LATIN,           // 252
  X_HACKER,              // 253
  X_KLINGON,             // 254
  X_ELMER_FUDD,          // 255
};

// Subscripted by PLang, for ULScript != Latn
extern const uint16 kPLangToLanguageOthr[256] = {
  UNKNOWN_LANGUAGE,      // 0
  HEBREW,                // 1
  JAPANESE,              // 2
  KOREAN,                // 3
  RUSSIAN,               // 4
  CHINESE,               // 5
  GREEK,                 // 6
  BULGARIAN,             // 7
  UKRAINIAN,             // 8
  HINDI,                 // 9
  MACEDONIAN,            // 10
  BENGALI,               // 11
  MALAYALAM,             // 12
  NEPALI,                // 13
  TELUGU,                // 14
  TAMIL,                 // 15
  BELARUSIAN,            // 16
  ROMANIAN,              // 17
  URDU,                  // 18
  BIHARI,                // 19
  TG_UNKNOWN_LANGUAGE,   // 20
  UNKNOWN_LANGUAGE,      // 21  (updated 2013.09.07 dsites)
  THAI,                  // 22
  SERBIAN,               // 23
  ARABIC,                // 24
  KANNADA,               // 25
  TAGALOG,               // 26
  PUNJABI,               // 27
  MARATHI,               // 28
  CHINESE_T,             // 29
  AMHARIC,               // 30
  GEORGIAN,              // 31
  TIGRINYA,              // 32
  PERSIAN,               // 33
  SINHALESE,             // 34
  KYRGYZ,                // 35
  YIDDISH,               // 36
  MONGOLIAN,             // 37
  ARMENIAN,              // 38
  LAOTHIAN,              // 39
  SINDHI,                // 40
  KHMER,                 // 41
  TIBETAN,               // 42
  DHIVEHI,               // 43
  CHEROKEE,              // 44
  SYRIAC,                // 45
  LIMBU,                 // 46
  ORIYA,                 // 47
  UZBEK,                 // 48
  AZERBAIJANI,           // 49
  BOSNIAN,               // 50
  ASSAMESE,              // 51
  PASHTO,                // 52
  TAJIK,                 // 53
  ABKHAZIAN,             // 54
  BASHKIR,               // 55
  TURKMEN,               // 56
  DZONGKHA,              // 57
  INUKTITUT,             // 58
  KASHMIRI,              // 59
  UIGHUR,                // 60
  KURDISH,               // 61
  NEWARI,                // 62
  OSSETIAN,              // 63
  RAJASTHANI,            // 64
  BURMESE,               // 65
  UNKNOWN_LANGUAGE,      // 66
  UNKNOWN_LANGUAGE,      // 67
  KAZAKH,                // 68
  UNKNOWN_LANGUAGE,      // 69
  UNKNOWN_LANGUAGE,      // 70
  UNKNOWN_LANGUAGE,      // 71
  TATAR,                 // 72
  UNKNOWN_LANGUAGE,      // 73
  UNKNOWN_LANGUAGE,      // 74
  UNKNOWN_LANGUAGE,      // 75
  UNKNOWN_LANGUAGE,      // 76
  UNKNOWN_LANGUAGE,      // 77
  UNKNOWN_LANGUAGE,      // 78
  UNKNOWN_LANGUAGE,      // 79
  UNKNOWN_LANGUAGE,      // 80
  UNKNOWN_LANGUAGE,      // 81
  HAUSA,                 // 82
  UNKNOWN_LANGUAGE,      // 83
  UNKNOWN_LANGUAGE,      // 84
  UNKNOWN_LANGUAGE,      // 85
  UNKNOWN_LANGUAGE,      // 86
  UNKNOWN_LANGUAGE,      // 87
  UNKNOWN_LANGUAGE,      // 88
  UNKNOWN_LANGUAGE,      // 89
  UNKNOWN_LANGUAGE,      // 90
  UNKNOWN_LANGUAGE,      // 91
  SANSKRIT,              // 92
  UNKNOWN_LANGUAGE,      // 93
  UNKNOWN_LANGUAGE,      // 94
  UNKNOWN_LANGUAGE,      // 95
  UNKNOWN_LANGUAGE,      // 96
  ZHUANG,                // 97
  UNKNOWN_LANGUAGE,      // 98
  UNKNOWN_LANGUAGE,      // 99
  UNKNOWN_LANGUAGE,      // 100
  UNKNOWN_LANGUAGE,      // 101
  UNKNOWN_LANGUAGE,      // 102
  UNKNOWN_LANGUAGE,      // 103
  UNKNOWN_LANGUAGE,      // 104
  UNKNOWN_LANGUAGE,      // 105
  UNKNOWN_LANGUAGE,      // 106
  UNKNOWN_LANGUAGE,      // 107
  UNKNOWN_LANGUAGE,      // 108
  UNKNOWN_LANGUAGE,      // 109
  UNKNOWN_LANGUAGE,      // 110
  UNKNOWN_LANGUAGE,      // 111
  UNKNOWN_LANGUAGE,      // 112
  UNKNOWN_LANGUAGE,      // 113
  UNKNOWN_LANGUAGE,      // 114
  UNKNOWN_LANGUAGE,      // 115
  UNKNOWN_LANGUAGE,      // 116
  UNKNOWN_LANGUAGE,      // 117
  UNKNOWN_LANGUAGE,      // 118
  UNKNOWN_LANGUAGE,      // 119
  UNKNOWN_LANGUAGE,      // 120
  UNKNOWN_LANGUAGE,      // 121
  UNKNOWN_LANGUAGE,      // 122
  UNKNOWN_LANGUAGE,      // 123
  UNKNOWN_LANGUAGE,      // 124
  UNKNOWN_LANGUAGE,      // 125
  UNKNOWN_LANGUAGE,      // 126
  UNKNOWN_LANGUAGE,      // 127
  UNKNOWN_LANGUAGE,      // 128
  UNKNOWN_LANGUAGE,      // 129
  UNKNOWN_LANGUAGE,      // 130
  UNKNOWN_LANGUAGE,      // 131
  UNKNOWN_LANGUAGE,      // 132
  UNKNOWN_LANGUAGE,      // 133
  UNKNOWN_LANGUAGE,      // 134
  UNKNOWN_LANGUAGE,      // 135
  UNKNOWN_LANGUAGE,      // 136
  UNKNOWN_LANGUAGE,      // 137
  UNKNOWN_LANGUAGE,      // 138
  UNKNOWN_LANGUAGE,      // 139
  UNKNOWN_LANGUAGE,      // 140
  UNKNOWN_LANGUAGE,      // 141
  UNKNOWN_LANGUAGE,      // 142
  UNKNOWN_LANGUAGE,      // 143
  UNKNOWN_LANGUAGE,      // 144
  UNKNOWN_LANGUAGE,      // 145
  UNKNOWN_LANGUAGE,      // 146
  UNKNOWN_LANGUAGE,      // 147
  UNKNOWN_LANGUAGE,      // 148
  UNKNOWN_LANGUAGE,      // 149
  UNKNOWN_LANGUAGE,      // 150
  UNKNOWN_LANGUAGE,      // 151
  UNKNOWN_LANGUAGE,      // 152
  UNKNOWN_LANGUAGE,      // 153
  UNKNOWN_LANGUAGE,      // 154
  UNKNOWN_LANGUAGE,      // 155
  UNKNOWN_LANGUAGE,      // 156
  UNKNOWN_LANGUAGE,      // 157
  UNKNOWN_LANGUAGE,      // 158
  UNKNOWN_LANGUAGE,      // 159
  UNKNOWN_LANGUAGE,      // 160
  UNKNOWN_LANGUAGE,      // 161
  UNKNOWN_LANGUAGE,      // 162
  UNKNOWN_LANGUAGE,      // 163
  UNKNOWN_LANGUAGE,      // 164
  UNKNOWN_LANGUAGE,      // 165
  UNKNOWN_LANGUAGE,      // 166
  UNKNOWN_LANGUAGE,      // 167
  UNKNOWN_LANGUAGE,      // 168
  UNKNOWN_LANGUAGE,      // 169
  UNKNOWN_LANGUAGE,      // 170
  UNKNOWN_LANGUAGE,      // 171
  UNKNOWN_LANGUAGE,      // 172
  UNKNOWN_LANGUAGE,      // 173
  UNKNOWN_LANGUAGE,      // 174
  UNKNOWN_LANGUAGE,      // 175
  UNKNOWN_LANGUAGE,      // 176
  UNKNOWN_LANGUAGE,      // 177
  UNKNOWN_LANGUAGE,      // 178
  UNKNOWN_LANGUAGE,      // 179
  UNKNOWN_LANGUAGE,      // 180
  UNKNOWN_LANGUAGE,      // 181
  UNKNOWN_LANGUAGE,      // 182
  UNKNOWN_LANGUAGE,      // 183
  UNKNOWN_LANGUAGE,      // 184
  UNKNOWN_LANGUAGE,      // 185
  UNKNOWN_LANGUAGE,      // 186
  UNKNOWN_LANGUAGE,      // 187
  UNKNOWN_LANGUAGE,      // 188
  UNKNOWN_LANGUAGE,      // 189
  UNKNOWN_LANGUAGE,      // 190
  UNKNOWN_LANGUAGE,      // 191
  UNKNOWN_LANGUAGE,      // 192
  UNKNOWN_LANGUAGE,      // 193
  UNKNOWN_LANGUAGE,      // 194
  UNKNOWN_LANGUAGE,      // 195
  UNKNOWN_LANGUAGE,      // 196
  UNKNOWN_LANGUAGE,      // 197
  UNKNOWN_LANGUAGE,      // 198
  UNKNOWN_LANGUAGE,      // 199
  UNKNOWN_LANGUAGE,      // 200
  UNKNOWN_LANGUAGE,      // 201
  UNKNOWN_LANGUAGE,      // 202
  UNKNOWN_LANGUAGE,      // 203
  UNKNOWN_LANGUAGE,      // 204
  UNKNOWN_LANGUAGE,      // 205
  UNKNOWN_LANGUAGE,      // 206
  UNKNOWN_LANGUAGE,      // 207
  UNKNOWN_LANGUAGE,      // 208
  UNKNOWN_LANGUAGE,      // 209
  UNKNOWN_LANGUAGE,      // 210
  UNKNOWN_LANGUAGE,      // 211
  UNKNOWN_LANGUAGE,      // 212
  UNKNOWN_LANGUAGE,      // 213
  UNKNOWN_LANGUAGE,      // 214
  UNKNOWN_LANGUAGE,      // 215
  UNKNOWN_LANGUAGE,      // 216
  UNKNOWN_LANGUAGE,      // 217
  UNKNOWN_LANGUAGE,      // 218
  UNKNOWN_LANGUAGE,      // 219
  UNKNOWN_LANGUAGE,      // 220
  UNKNOWN_LANGUAGE,      // 221
  UNKNOWN_LANGUAGE,      // 222
  UNKNOWN_LANGUAGE,      // 223
  UNKNOWN_LANGUAGE,      // 224
  UNKNOWN_LANGUAGE,      // 225
  UNKNOWN_LANGUAGE,      // 226
  UNKNOWN_LANGUAGE,      // 227
  UNKNOWN_LANGUAGE,      // 228
  UNKNOWN_LANGUAGE,      // 229
  UNKNOWN_LANGUAGE,      // 230
  UNKNOWN_LANGUAGE,      // 231
  UNKNOWN_LANGUAGE,      // 232
  UNKNOWN_LANGUAGE,      // 233
  UNKNOWN_LANGUAGE,      // 234
  UNKNOWN_LANGUAGE,      // 235
  UNKNOWN_LANGUAGE,      // 236
  UNKNOWN_LANGUAGE,      // 237
  UNKNOWN_LANGUAGE,      // 238
  UNKNOWN_LANGUAGE,      // 239
  UNKNOWN_LANGUAGE,      // 240
  UNKNOWN_LANGUAGE,      // 241
  UNKNOWN_LANGUAGE,      // 242
  UNKNOWN_LANGUAGE,      // 243
  UNKNOWN_LANGUAGE,      // 244
  UNKNOWN_LANGUAGE,      // 245
  UNKNOWN_LANGUAGE,      // 246
  UNKNOWN_LANGUAGE,      // 247
  UNKNOWN_LANGUAGE,      // 248
  UNKNOWN_LANGUAGE,      // 249
  UNKNOWN_LANGUAGE,      // 250
  UNKNOWN_LANGUAGE,      // 251
  UNKNOWN_LANGUAGE,      // 252
  UNKNOWN_LANGUAGE,      // 253
  UNKNOWN_LANGUAGE,      // 254
  UNKNOWN_LANGUAGE,      // 255
};

// Subscripted by PLang, for ULScript = Latn
extern const uint8 kPLangToCloseSetLatn[256] = {
  0,  // 0
  0,  // 1
  7,  // 2 da
  0,  // 3
  0,  // 4
  0,  // 5
  0,  // 6
  0,  // 7
  7,  // 8 no
  0,  // 9
  8,  // 10 pt
  8,  // 11 es
  0,  // 12
  3,  // 13 cs
  0,  // 14
  0,  // 15
  0,  // 16
  0,  // 17
  0,  // 18
  0,  // 19
  0,  // 20
  0,  // 21
  5,  // 22 hr
  5,  // 23 sr
  0,  // 24
  8,  // 25 gl
  0,  // 26
  0,  // 27
  1,  // 28 id
  0,  // 29
  1,  // 30 ms
  0,  // 31
  0,  // 32
  0,  // 33
  0,  // 34
  0,  // 35
  0,  // 36
  0,  // 37
  0,  // 38
  0,  // 39
  0,  // 40
  0,  // 41
  0,  // 42
  0,  // 43
  0,  // 44
  3,  // 45 sk
  0,  // 46
  0,  // 47
  0,  // 48
  0,  // 49
  0,  // 50
  7,  // 51 nn
  4,  // 52 xh
  4,  // 53 zu
  0,  // 54
  0,  // 55
  0,  // 56
  0,  // 57
  0,  // 58
  0,  // 59
  0,  // 60
  0,  // 61
  0,  // 62
  0,  // 63
  0,  // 64
  0,  // 65
  0,  // 66
  0,  // 67
  0,  // 68
  0,  // 69
  0,  // 70
  0,  // 71
  0,  // 72
  0,  // 73
  0,  // 74
  0,  // 75
  0,  // 76
  0,  // 77
  0,  // 78
  0,  // 79
  0,  // 80
  0,  // 81
  0,  // 82
  0,  // 83
  0,  // 84
  9,  // 85 rw
  0,  // 86
  0,  // 87
  0,  // 88
  9,  // 89 rn
  0,  // 90
  0,  // 91
  0,  // 92
  0,  // 93
  0,  // 94
  0,  // 95
  0,  // 96
  0,  // 97
  0,  // 98
  0,  // 99
  0,  // 100
  0,  // 101
  0,  // 102
  0,  // 103
  0,  // 104
  0,  // 105
  0,  // 106
  0,  // 107
  0,  // 108
  0,  // 109
  0,  // 110
  0,  // 111
  0,  // 112
  0,  // 113
  0,  // 114
  0,  // 115
  0,  // 116
  0,  // 117
  0,  // 118
  0,  // 119
  0,  // 120
  0,  // 121
  0,  // 122
  0,  // 123
  0,  // 124
  0,  // 125
  0,  // 126
  0,  // 127
  0,  // 128
  0,  // 129
  0,  // 130
  0,  // 131
  0,  // 132
  0,  // 133
  0,  // 134
  0,  // 135
  0,  // 136
  0,  // 137
  0,  // 138
  0,  // 139
  0,  // 140
  0,  // 141
  0,  // 142
  0,  // 143
  0,  // 144
  0,  // 145
  0,  // 146
  0,  // 147
  0,  // 148
  0,  // 149
  0,  // 150
  0,  // 151
  0,  // 152
  0,  // 153
  0,  // 154
  0,  // 155
  0,  // 156
  0,  // 157
  0,  // 158
  0,  // 159
  0,  // 160
  0,  // 161
  0,  // 162
  0,  // 163
  0,  // 164
  0,  // 165
  0,  // 166
  0,  // 167
  0,  // 168
  0,  // 169
  0,  // 170
  0,  // 171
  0,  // 172
  0,  // 173
  0,  // 174
  0,  // 175
  0,  // 176
  0,  // 177
  0,  // 178
  0,  // 179
  0,  // 180
  0,  // 181
  0,  // 182
  0,  // 183
  0,  // 184
  0,  // 185
  0,  // 186
  0,  // 187
  0,  // 188
  0,  // 189
  0,  // 190
  0,  // 191
  0,  // 192
  0,  // 193
  0,  // 194
  0,  // 195
  0,  // 196
  0,  // 197
  0,  // 198
  0,  // 199
  0,  // 200
  0,  // 201
  0,  // 202
  0,  // 203
  0,  // 204
  0,  // 205
  0,  // 206
  0,  // 207
  0,  // 208
  0,  // 209
  0,  // 210
  0,  // 211
  0,  // 212
  0,  // 213
  0,  // 214
  0,  // 215
  0,  // 216
  0,  // 217
  0,  // 218
  0,  // 219
  0,  // 220
  0,  // 221
  0,  // 222
  0,  // 223
  0,  // 224
  0,  // 225
  0,  // 226
  0,  // 227
  0,  // 228
  0,  // 229
  0,  // 230
  0,  // 231
  0,  // 232
  0,  // 233
  0,  // 234
  0,  // 235
  0,  // 236
  0,  // 237
  0,  // 238
  0,  // 239
  0,  // 240
  0,  // 241
  0,  // 242
  0,  // 243
  0,  // 244
  0,  // 245
  0,  // 246
  0,  // 247
  0,  // 248
  0,  // 249
  0,  // 250
  0,  // 251
  0,  // 252
  0,  // 253
  0,  // 254
  0,  // 255
};

// Subscripted by PLang, for ULScript != Latn
extern const uint8 kPLangToCloseSetOthr[256] = {
  0,  // 0
  0,  // 1
  0,  // 2
  0,  // 3
  0,  // 4
  0,  // 5
  0,  // 6
  0,  // 7
  0,  // 8
  6,  // 9 hi
  0,  // 10
  0,  // 11
  0,  // 12
  6,  // 13 ne
  0,  // 14
  0,  // 15
  0,  // 16
  0,  // 17
  0,  // 18
  6,  // 19 bh
  0,  // 20
  0,  // 21
  0,  // 22
  0,  // 23
  0,  // 24
  0,  // 25
  0,  // 26
  0,  // 27
  6,  // 28 mr
  0,  // 29
  0,  // 30
  0,  // 31
  0,  // 32
  0,  // 33
  0,  // 34
  0,  // 35
  0,  // 36
  0,  // 37
  0,  // 38
  0,  // 39
  0,  // 40
  0,  // 41
  2,  // 42 bo
  0,  // 43
  0,  // 44
  0,  // 45
  0,  // 46
  0,  // 47
  0,  // 48
  0,  // 49
  0,  // 50
  0,  // 51
  0,  // 52
  0,  // 53
  0,  // 54
  0,  // 55
  0,  // 56
  2,  // 57 dz
  0,  // 58
  0,  // 59
  0,  // 60
  0,  // 61
  0,  // 62
  0,  // 63
  0,  // 64
  0,  // 65
  0,  // 66
  0,  // 67
  0,  // 68
  0,  // 69
  0,  // 70
  0,  // 71
  0,  // 72
  0,  // 73
  0,  // 74
  0,  // 75
  0,  // 76
  0,  // 77
  0,  // 78
  0,  // 79
  0,  // 80
  0,  // 81
  0,  // 82
  0,  // 83
  0,  // 84
  0,  // 85
  0,  // 86
  0,  // 87
  0,  // 88
  0,  // 89
  0,  // 90
  0,  // 91
  0,  // 92
  0,  // 93
  0,  // 94
  0,  // 95
  0,  // 96
  0,  // 97
  0,  // 98
  0,  // 99
  0,  // 100
  0,  // 101
  0,  // 102
  0,  // 103
  0,  // 104
  0,  // 105
  0,  // 106
  0,  // 107
  0,  // 108
  0,  // 109
  0,  // 110
  0,  // 111
  0,  // 112
  0,  // 113
  0,  // 114
  0,  // 115
  0,  // 116
  0,  // 117
  0,  // 118
  0,  // 119
  0,  // 120
  0,  // 121
  0,  // 122
  0,  // 123
  0,  // 124
  0,  // 125
  0,  // 126
  0,  // 127
  0,  // 128
  0,  // 129
  0,  // 130
  0,  // 131
  0,  // 132
  0,  // 133
  0,  // 134
  0,  // 135
  0,  // 136
  0,  // 137
  0,  // 138
  0,  // 139
  0,  // 140
  0,  // 141
  0,  // 142
  0,  // 143
  0,  // 144
  0,  // 145
  0,  // 146
  0,  // 147
  0,  // 148
  0,  // 149
  0,  // 150
  0,  // 151
  0,  // 152
  0,  // 153
  0,  // 154
  0,  // 155
  0,  // 156
  0,  // 157
  0,  // 158
  0,  // 159
  0,  // 160
  0,  // 161
  0,  // 162
  0,  // 163
  0,  // 164
  0,  // 165
  0,  // 166
  0,  // 167
  0,  // 168
  0,  // 169
  0,  // 170
  0,  // 171
  0,  // 172
  0,  // 173
  0,  // 174
  0,  // 175
  0,  // 176
  0,  // 177
  0,  // 178
  0,  // 179
  0,  // 180
  0,  // 181
  0,  // 182
  0,  // 183
  0,  // 184
  0,  // 185
  0,  // 186
  0,  // 187
  0,  // 188
  0,  // 189
  0,  // 190
  0,  // 191
  0,  // 192
  0,  // 193
  0,  // 194
  0,  // 195
  0,  // 196
  0,  // 197
  0,  // 198
  0,  // 199
  0,  // 200
  0,  // 201
  0,  // 202
  0,  // 203
  0,  // 204
  0,  // 205
  0,  // 206
  0,  // 207
  0,  // 208
  0,  // 209
  0,  // 210
  0,  // 211
  0,  // 212
  0,  // 213
  0,  // 214
  0,  // 215
  0,  // 216
  0,  // 217
  0,  // 218
  0,  // 219
  0,  // 220
  0,  // 221
  0,  // 222
  0,  // 223
  0,  // 224
  0,  // 225
  0,  // 226
  0,  // 227
  0,  // 228
  0,  // 229
  0,  // 230
  0,  // 231
  0,  // 232
  0,  // 233
  0,  // 234
  0,  // 235
  0,  // 236
  0,  // 237
  0,  // 238
  0,  // 239
  0,  // 240
  0,  // 241
  0,  // 242
  0,  // 243
  0,  // 244
  0,  // 245
  0,  // 246
  0,  // 247
  0,  // 248
  0,  // 249
  0,  // 250
  0,  // 251
  0,  // 252
  0,  // 253
  0,  // 254
  0,  // 255
};

// Alphabetical order for binary search
extern const int kNameToLanguageSize = 304;
extern const CharIntPair kNameToLanguage[kNameToLanguageSize] = {
  {"ABKHAZIAN",            130},  // ab
  {"AFAR",                 131},  // aa
  {"AFRIKAANS",            101},  // af
  {"AKAN",                 161},  // ak
  {"ALBANIAN",              45},  // sq
  {"AMHARIC",               73},  // am
  {"ARABIC",                54},  // ar
  {"ARMENIAN",              97},  // hy
  {"ASSAMESE",             111},  // as
  {"AYMARA",               132},  // ay
  {"AZERBAIJANI",           74},  // az
  {"BASHKIR",              133},  // ba
  {"BASQUE",                57},  // eu
  {"BELARUSIAN",            47},  // be
  {"BENGALI",               37},  // bn
  {"BIHARI",                51},  // bh
  {"BISLAMA",              134},  // bi
  {"BOSNIAN",               78},  // bs
  {"BRETON",                89},  // br
  {"BULGARIAN",             27},  // bg
  {"BURMESE",              103},  // my
  {"CATALAN",               55},  // ca
  {"CEBUANO",              165},  // ceb
  {"CHEROKEE",             107},  // chr
  {"CHICHEWA",             174},  // ny
  {"CORSICAN",             112},  // co
  {"CROATIAN",              28},  // hr
  {"CROATIAN",              28},  // sh-Latn
  {"CZECH",                 17},  // cs
  {"Chinese",               16},  // zh-CN
  {"Chinese",               16},  // zh-Hans
  {"Chinese",               16},  // zh-Hani
  {"Chinese",               16},  // zh
  {"ChineseT",              69},  // zht
  {"ChineseT",              69},  // zhT
  {"ChineseT",              69},  // zh-SG
  {"ChineseT",              69},  // zh-HK
  {"ChineseT",              69},  // zh-TW
  {"ChineseT",              69},  // zh-Hant
  {"DANISH",                 1},  // da
  {"DHIVEHI",              106},  // dv
  {"DUTCH",                  2},  // nl
  {"DZONGKHA",             135},  // dz
  {"ENGLISH",                0},  // en
  {"ESPERANTO",             56},  // eo
  {"ESTONIAN",              24},  // et
  {"EWE",                  166},  // ee
  {"FAROESE",               70},  // fo
  {"FIJIAN",               136},  // fj
  {"FINNISH",                3},  // fi
  {"FRENCH",                 4},  // fr
  {"FRISIAN",               67},  // fy
  {"GA",                   167},  // gaa
  {"GALICIAN",              31},  // gl
  {"GANDA",                158},  // lg
  {"GEORGIAN",              75},  // ka
  {"GERMAN",                 5},  // de
  {"GREEK",                 18},  // el
  {"GREENLANDIC",          137},  // kl
  {"GUARANI",               85},  // gn
  {"GUJARATI",              52},  // gu
  {"HAITIAN_CREOLE",       139},  // ht
  {"HAUSA",                138},  // ha
  {"HAWAIIAN",             164},  // haw
  {"HEBREW",                 6},  // he
  {"HEBREW",                 6},  // iw
  {"HINDI",                 35},  // hi
  {"HMONG",                168},  // hmn
  {"HUNGARIAN",             23},  // hu
  {"ICELANDIC",             19},  // is
  {"IGBO",                 162},  // ig
  {"INDONESIAN",            38},  // id
  {"INTERLINGUA",           58},  // ia
  {"INTERLINGUE",          113},  // ie
  {"INUKTITUT",            141},  // iu
  {"INUPIAK",              140},  // ik
  {"IRISH",                 30},  // ga
  {"ITALIAN",                7},  // it
  {"Ignore",                25},  // xxx
  {"JAVANESE",              48},  // jv
  {"JAVANESE",              48},  // jw
  {"Japanese",               8},  // ja
  {"KANNADA",               59},  // kn
  {"KASHMIRI",             142},  // ks
  {"KAZAKH",               114},  // kk
  {"KHASI",                156},  // kha
  {"KHMER",                104},  // km
  {"KINYARWANDA",          143},  // rw
  {"KRIO",                 169},  // kri
  {"KURDISH",               95},  // ku
  {"KYRGYZ",                88},  // ky
  {"Korean",                 9},  // ko
  {"LAOTHIAN",              98},  // lo
  {"LATIN",                 39},  // la
  {"LATVIAN",               20},  // lv
  {"LIMBU",                109},  // sit-Limb
  {"LIMBU",                109},  // sit-NP
  {"LIMBU",                109},  // lif
  {"LINGALA",              115},  // ln
  {"LITHUANIAN",            21},  // lt
  {"LOZI",                 170},  // loz
  {"LUBA_LULUA",           171},  // lua
  {"LUO_KENYA_AND_TANZANIA", 172},  // luo
  {"LUXEMBOURGISH",        102},  // lb
  {"MACEDONIAN",            36},  // mk
  {"MALAGASY",             144},  // mg
  {"MALAY",                 40},  // ms
  {"MALAYALAM",             41},  // ml
  {"MALTESE",               65},  // mt
  {"MANX",                 159},  // gv
  {"MAORI",                128},  // mi
  {"MARATHI",               64},  // mr
  {"MAURITIAN_CREOLE",     163},  // mfe
  {"MOLDAVIAN",             22},  // mo
  {"MONGOLIAN",             96},  // mn
  {"MONTENEGRIN",          160},  // srm
  {"MONTENEGRIN",          160},  // sr-Latn-ME
  {"MONTENEGRIN",          160},  // sr-ME
  {"MONTENEGRIN",          160},  // srM
  {"NAURU",                145},  // na
  {"NDEBELE",              506},  // nr
  {"NEPALI",                43},  // ne
  {"NEWARI",               173},  // new
  {"NORWEGIAN",             10},  // nb
  {"NORWEGIAN",             10},  // no
  {"NORWEGIAN_N",           80},  // nn
  {"NYANJA",               174},  // ny
  {"OCCITAN",               49},  // oc
  {"ORIYA",                110},  // or
  {"OROMO",                146},  // om
  {"OSSETIAN",             175},  // os
  {"PAMPANGA",             176},  // pam
  {"PASHTO",               117},  // ps
  {"PEDI",                 177},  // nso
  {"PERSIAN",               77},  // fa
  {"POLISH",                11},  // pl
  {"PORTUGUESE",            12},  // pt
  {"PUNJABI",               60},  // pa
  {"QUECHUA",              118},  // qu
  {"RAJASTHANI",           178},  // raj
  {"RHAETO_ROMANCE",       100},  // rm
  {"ROMANIAN",              22},  // ro
  {"RUNDI",                147},  // rn
  {"RUSSIAN",               13},  // ru
  {"SAMOAN",               148},  // sm
  {"SANGO",                149},  // sg
  {"SANSKRIT",             150},  // sa
  {"SCOTS",                157},  // sco
  {"SCOTS_GAELIC",          61},  // gd
  {"SERBIAN",               29},  // sh-Cyrl
  {"SERBIAN",               29},  // sr
  {"SESELWA",              179},  // crs
  {"SESELWA_CREOLE_FRENCH", 179},  // crs
  {"SESOTHO",               86},  // st
  {"SHONA",                119},  // sn
  {"SINDHI",                99},  // sd
  {"SINHALESE",             79},  // si
  {"SISWANT",              151},  // ss
  {"SLOVAK",                68},  // sk
  {"SLOVENIAN",             63},  // sl
  {"SOMALI",                93},  // so
  {"SPANISH",               14},  // es
  {"SUNDANESE",             71},  // su
  {"SWAHILI",               62},  // sw
  {"SWEDISH",               15},  // sv
  {"SYRIAC",               108},  // syr
  {"TAGALOG",               32},  // tl
  {"TAJIK",                120},  // tg
  {"TAMIL",                 46},  // ta
  {"TATAR",                121},  // tt
  {"TELUGU",                44},  // te
  {"THAI",                  53},  // th
  {"TIBETAN",              105},  // bo
  {"TIGRINYA",              76},  // ti
  {"TONGA",                122},  // to
  {"TSONGA",               152},  // ts
  {"TSWANA",               153},  // tn
  {"TUMBUKA",              180},  // tum
  {"TURKISH",               33},  // tr
  {"TURKMEN",               87},  // tk
  {"TWI",                   90},  // tw
  {"UIGHUR",                94},  // ug
  {"UKRAINIAN",             34},  // uk
  {"URDU",                  50},  // ur
  {"UZBEK",                 72},  // uz
  {"Unknown",               26},  // un
  {"VENDA",                181},  // ve
  {"VIETNAMESE",            66},  // vi
  {"VOLAPUK",              154},  // vo
  {"WARAY_PHILIPPINES",    182},  // war
  {"WELSH",                 42},  // cy
  {"WOLOF",                129},  // wo
  {"XHOSA",                 83},  // xh
  {"X_Arabic",             518},  // xx-Arab
  {"X_Armenian",           516},  // xx-Armn
  {"X_Avestan",            591},  // xx-Avst
  {"X_BORK_BORK_BORK",     507},  // zzb
  {"X_Balinese",           573},  // xx-Bali
  {"X_Bamum",              595},  // xx-Bamu
  {"X_Batak",              604},  // xx-Batk
  {"X_Bengali",            522},  // xx-Beng
  {"X_Bopomofo",           546},  // xx-Bopo
  {"X_Brahmi",             605},  // xx-Brah
  {"X_Braille",            564},  // xx-Brai
  {"X_Buginese",           565},  // xx-Bugi
  {"X_Buhid",              555},  // xx-Buhd
  {"X_Canadian_Aboriginal", 539},  // xx-Cans
  {"X_Carian",             586},  // xx-Cari
  {"X_Chakma",             607},  // xx-Cakm
  {"X_Cham",               588},  // xx-Cham
  {"X_Cherokee",           538},  // xx-Cher
  {"X_Common",             512},  // xx-Zyyy
  {"X_Coptic",             566},  // xx-Copt
  {"X_Cuneiform",          574},  // xx-Xsux
  {"X_Cypriot",            563},  // xx-Cprt
  {"X_Cyrillic",           515},  // xx-Cyrl
  {"X_Deseret",            551},  // xx-Dsrt
  {"X_Devanagari",         521},  // xx-Deva
  {"X_ELMER_FUDD",         511},  // zze
  {"X_Egyptian_Hieroglyphs", 592},  // xx-Egyp
  {"X_Ethiopic",           537},  // xx-Ethi
  {"X_Georgian",           535},  // xx-Geor
  {"X_Glagolitic",         568},  // xx-Glag
  {"X_Gothic",             550},  // xx-Goth
  {"X_Greek",              514},  // xx-Grek
  {"X_Gujarati",           524},  // xx-Gujr
  {"X_Gurmukhi",           523},  // xx-Guru
  {"X_HACKER",             509},  // zzh
  {"X_Han",                547},  // xx-Hani
  {"X_Hangul",             536},  // xx-Hang
  {"X_Hanunoo",            554},  // xx-Hano
  {"X_Hebrew",             517},  // xx-Hebr
  {"X_Hiragana",           544},  // xx-Hira
  {"X_Imperial_Aramaic",   598},  // xx-Armi
  {"X_Inherited",          552},  // xx-Qaai
  {"X_Inscriptional_Pahlavi", 601},  // xx-Phli
  {"X_Inscriptional_Parthian", 600},  // xx-Prti
  {"X_Javanese",           596},  // xx-Java
  {"X_KLINGON",            510},  // tlh
  {"X_Kaithi",             603},  // xx-Kthi
  {"X_Kannada",            528},  // xx-Knda
  {"X_Katakana",           545},  // xx-Kana
  {"X_Kayah_Li",           583},  // xx-Kali
  {"X_Kharoshthi",         572},  // xx-Khar
  {"X_Khmer",              542},  // xx-Khmr
  {"X_Lao",                532},  // xx-Laoo
  {"X_Latin",              513},  // xx-Latn
  {"X_Lepcha",             579},  // xx-Lepc
  {"X_Limbu",              557},  // xx-Limb
  {"X_Linear_B",           559},  // xx-Linb
  {"X_Lisu",               594},  // xx-Lisu
  {"X_Lycian",             585},  // xx-Lyci
  {"X_Lydian",             587},  // xx-Lydi
  {"X_Malayalam",          529},  // xx-Mlym
  {"X_Mandaic",            606},  // xx-Mand
  {"X_Meetei_Mayek",       597},  // xx-Mtei
  {"X_Meroitic_Cursive",   608},  // xx-Merc
  {"X_Meroitic_Hieroglyphs", 609},  // xx-Mero
  {"X_Miao",               610},  // xx-Plrd
  {"X_Mongolian",          543},  // xx-Mong
  {"X_Myanmar",            534},  // xx-Mymr
  {"X_New_Tai_Lue",        567},  // xx-Talu
  {"X_Nko",                577},  // xx-Nkoo
  {"X_Ogham",              540},  // xx-Ogam
  {"X_Ol_Chiki",           580},  // xx-Olck
  {"X_Old_Italic",         549},  // xx-Ital
  {"X_Old_Persian",        571},  // xx-Xpeo
  {"X_Old_South_Arabian",  599},  // xx-Sarb
  {"X_Old_Turkic",         602},  // xx-Orkh
  {"X_Oriya",              525},  // xx-Orya
  {"X_Osmanya",            562},  // xx-Osma
  {"X_PIG_LATIN",          508},  // zzp
  {"X_Phags_Pa",           576},  // xx-Phag
  {"X_Phoenician",         575},  // xx-Phnx
  {"X_Rejang",             584},  // xx-Rjng
  {"X_Runic",              541},  // xx-Runr
  {"X_Samaritan",          593},  // xx-Samr
  {"X_Saurashtra",         582},  // xx-Saur
  {"X_Sharada",            611},  // xx-Shrd
  {"X_Shavian",            561},  // xx-Shaw
  {"X_Sinhala",            530},  // xx-Sinh
  {"X_Sora_Sompeng",       612},  // xx-Sora
  {"X_Sundanese",          578},  // xx-Sund
  {"X_Syloti_Nagri",       570},  // xx-Sylo
  {"X_Syriac",             519},  // xx-Syrc
  {"X_Tagalog",            553},  // xx-Tglg
  {"X_Tagbanwa",           556},  // xx-Tagb
  {"X_Tai_Le",             558},  // xx-Tale
  {"X_Tai_Tham",           589},  // xx-Lana
  {"X_Tai_Viet",           590},  // xx-Tavt
  {"X_Takri",              613},  // xx-Takr
  {"X_Tamil",              526},  // xx-Taml
  {"X_Telugu",             527},  // xx-Telu
  {"X_Thaana",             520},  // xx-Thaa
  {"X_Thai",               531},  // xx-Thai
  {"X_Tibetan",            533},  // xx-Tibt
  {"X_Tifinagh",           569},  // xx-Tfng
  {"X_Ugaritic",           560},  // xx-Ugar
  {"X_Vai",                581},  // xx-Vaii
  {"X_Yi",                 548},  // xx-Yiii
  {"YIDDISH",               91},  // yi
  {"YORUBA",               123},  // yo
  {"ZHUANG",               155},  // za
  {"ZULU",                  84},  // zu
};

// Alphabetical order for binary search
extern const int kCodeToLanguageSize = 304;
extern const CharIntPair kCodeToLanguage[kCodeToLanguageSize] = {
  {"aa",   131},  // aa
  {"ab",   130},  // ab
  {"af",   101},  // af
  {"ak",   161},  // ak
  {"am",    73},  // am
  {"ar",    54},  // ar
  {"as",   111},  // as
  {"ay",   132},  // ay
  {"az",    74},  // az
  {"ba",   133},  // ba
  {"be",    47},  // be
  {"bg",    27},  // bg
  {"bh",    51},  // bh
  {"bi",   134},  // bi
  //{"hmn",  168},  // hmn   used to be blu
  {"bn",    37},  // bn
  {"bo",   105},  // bo
  {"br",    89},  // br
  {"bs",    78},  // bs
  {"ca",    55},  // ca
  {"ceb",  165},  // ceb
  {"chr",  107},  // chr
  {"co",   112},  // co
  {"crs",  179},  // crs
  {"crs",  179},  // crs
  {"cs",    17},  // cs
  {"cy",    42},  // cy
  {"da",     1},  // da
  {"de",     5},  // de
  {"dv",   106},  // dv
  {"dz",   135},  // dz
  {"ee",   166},  // ee
  {"el",    18},  // el
  {"en",     0},  // en
  {"eo",    56},  // eo
  {"es",    14},  // es
  {"et",    24},  // et
  {"eu",    57},  // eu
  {"fa",    77},  // fa
  {"fi",     3},  // fi
  {"fj",   136},  // fj
  {"fo",    70},  // fo
  {"fr",     4},  // fr
  {"fy",    67},  // fy
  {"ga",    30},  // ga
  {"gaa",  167},  // gaa
  {"gd",    61},  // gd
  {"gl",    31},  // gl
  {"gn",    85},  // gn
  {"gu",    52},  // gu
  {"gv",   159},  // gv
  {"ha",   138},  // ha
  {"haw",  164},  // haw
  {"he",     6},  // he
  {"hi",    35},  // hi
  {"hmn",  168},  // hmn  used to be blu
  {"hr",    28},  // hr
  {"ht",   139},  // ht
  {"hu",    23},  // hu
  {"hy",    97},  // hy
  {"ia",    58},  // ia
  {"id",    38},  // id
  {"ie",   113},  // ie
  {"ig",   162},  // ig
  {"ik",   140},  // ik
  {"is",    19},  // is
  {"it",     7},  // it
  {"iu",   141},  // iu
  {"iw",     6},  // iw
  {"ja",     8},  // ja
  {"jv",    48},  // jv
  {"jw",    48},  // jw
  {"ka",    75},  // ka
  {"kha",  156},  // kha
  {"kk",   114},  // kk
  {"kl",   137},  // kl
  {"km",   104},  // km
  {"kn",    59},  // kn
  {"ko",     9},  // ko
  {"kri",  169},  // kri
  {"ks",   142},  // ks
  {"ku",    95},  // ku
  {"ky",    88},  // ky
  {"la",    39},  // la
  {"lb",   102},  // lb
  {"lg",   158},  // lg
  {"lif",  109},  // lif
  {"ln",   115},  // ln
  {"lo",    98},  // lo
  {"loz",  170},  // loz
  {"lt",    21},  // lt
  {"lua",  171},  // lua
  {"luo",  172},  // luo
  {"lv",    20},  // lv
  {"mfe",  163},  // mfe
  {"mg",   144},  // mg
  {"mi",   128},  // mi
  {"mk",    36},  // mk
  {"ml",    41},  // ml
  {"mn",    96},  // mn
  {"mo",    22},  // mo
  {"mr",    64},  // mr
  {"ms",    40},  // ms
  {"mt",    65},  // mt
  {"my",   103},  // my
  {"na",   145},  // na
  {"nb",    10},  // nb
  {"ne",    43},  // ne
  {"new",  173},  // new
  {"nl",     2},  // nl
  {"nn",    80},  // nn
  {"no",    10},  // no
  {"nr",   506},  // nr
  {"nso",  177},  // nso
  {"ny",   174},  // ny
  {"ny",   174},  // ny
  {"oc",    49},  // oc
  {"om",   146},  // om
  {"or",   110},  // or
  {"os",   175},  // os
  {"pa",    60},  // pa
  {"pam",  176},  // pam
  {"pl",    11},  // pl
  {"ps",   117},  // ps
  {"pt",    12},  // pt
  {"qu",   118},  // qu
  {"raj",  178},  // raj
  {"rm",   100},  // rm
  {"rn",   147},  // rn
  {"ro",    22},  // ro
  {"ru",    13},  // ru
  {"rw",   143},  // rw
  {"sa",   150},  // sa
  {"sco",  157},  // sco
  {"sd",    99},  // sd
  {"sg",   149},  // sg
  {"sh-Cyrl",  29},  // sh-Cyrl
  {"sh-Latn",  28},  // sh-Latn
  {"si",    79},  // si
  {"sit-Limb", 109},  // sit-Limb
  {"sit-NP", 109},  // sit-NP
  {"sk",    68},  // sk
  {"sl",    63},  // sl
  {"sm",   148},  // sm
  {"sn",   119},  // sn
  {"so",    93},  // so
  {"sq",    45},  // sq
  {"sr",    29},  // sr
  {"sr-Latn-ME", 160},  // sr-Latn-ME
  {"sr-ME", 160},  // sr-ME
  {"srM",  160},  // srM
  {"srm",  160},  // srm
  {"ss",   151},  // ss
  {"st",    86},  // st
  {"su",    71},  // su
  {"sv",    15},  // sv
  {"sw",    62},  // sw
  {"syr",  108},  // syr
  {"ta",    46},  // ta
  {"te",    44},  // te
  {"tg",   120},  // tg
  {"th",    53},  // th
  {"ti",    76},  // ti
  {"tk",    87},  // tk
  {"tl",    32},  // tl
  {"tlh",  510},  // tlh
  {"tn",   153},  // tn
  {"to",   122},  // to
  {"tr",    33},  // tr
  {"ts",   152},  // ts
  {"tt",   121},  // tt
  {"tum",  180},  // tum
  {"tw",    90},  // tw
  {"ug",    94},  // ug
  {"uk",    34},  // uk
  {"un",    26},  // un
  {"ur",    50},  // ur
  {"uz",    72},  // uz
  {"ve",   181},  // ve
  {"vi",    66},  // vi
  {"vo",   154},  // vo
  {"war",  182},  // war
  {"wo",   129},  // wo
  {"xh",    83},  // xh
  {"xx-Arab", 518},  // xx-Arab
  {"xx-Armi", 598},  // xx-Armi
  {"xx-Armn", 516},  // xx-Armn
  {"xx-Avst", 591},  // xx-Avst
  {"xx-Bali", 573},  // xx-Bali
  {"xx-Bamu", 595},  // xx-Bamu
  {"xx-Batk", 604},  // xx-Batk
  {"xx-Beng", 522},  // xx-Beng
  {"xx-Bopo", 546},  // xx-Bopo
  {"xx-Brah", 605},  // xx-Brah
  {"xx-Brai", 564},  // xx-Brai
  {"xx-Bugi", 565},  // xx-Bugi
  {"xx-Buhd", 555},  // xx-Buhd
  {"xx-Cakm", 607},  // xx-Cakm
  {"xx-Cans", 539},  // xx-Cans
  {"xx-Cari", 586},  // xx-Cari
  {"xx-Cham", 588},  // xx-Cham
  {"xx-Cher", 538},  // xx-Cher
  {"xx-Copt", 566},  // xx-Copt
  {"xx-Cprt", 563},  // xx-Cprt
  {"xx-Cyrl", 515},  // xx-Cyrl
  {"xx-Deva", 521},  // xx-Deva
  {"xx-Dsrt", 551},  // xx-Dsrt
  {"xx-Egyp", 592},  // xx-Egyp
  {"xx-Ethi", 537},  // xx-Ethi
  {"xx-Geor", 535},  // xx-Geor
  {"xx-Glag", 568},  // xx-Glag
  {"xx-Goth", 550},  // xx-Goth
  {"xx-Grek", 514},  // xx-Grek
  {"xx-Gujr", 524},  // xx-Gujr
  {"xx-Guru", 523},  // xx-Guru
  {"xx-Hang", 536},  // xx-Hang
  {"xx-Hani", 547},  // xx-Hani
  {"xx-Hano", 554},  // xx-Hano
  {"xx-Hebr", 517},  // xx-Hebr
  {"xx-Hira", 544},  // xx-Hira
  {"xx-Ital", 549},  // xx-Ital
  {"xx-Java", 596},  // xx-Java
  {"xx-Kali", 583},  // xx-Kali
  {"xx-Kana", 545},  // xx-Kana
  {"xx-Khar", 572},  // xx-Khar
  {"xx-Khmr", 542},  // xx-Khmr
  {"xx-Knda", 528},  // xx-Knda
  {"xx-Kthi", 603},  // xx-Kthi
  {"xx-Lana", 589},  // xx-Lana
  {"xx-Laoo", 532},  // xx-Laoo
  {"xx-Latn", 513},  // xx-Latn
  {"xx-Lepc", 579},  // xx-Lepc
  {"xx-Limb", 557},  // xx-Limb
  {"xx-Linb", 559},  // xx-Linb
  {"xx-Lisu", 594},  // xx-Lisu
  {"xx-Lyci", 585},  // xx-Lyci
  {"xx-Lydi", 587},  // xx-Lydi
  {"xx-Mand", 606},  // xx-Mand
  {"xx-Merc", 608},  // xx-Merc
  {"xx-Mero", 609},  // xx-Mero
  {"xx-Mlym", 529},  // xx-Mlym
  {"xx-Mong", 543},  // xx-Mong
  {"xx-Mtei", 597},  // xx-Mtei
  {"xx-Mymr", 534},  // xx-Mymr
  {"xx-Nkoo", 577},  // xx-Nkoo
  {"xx-Ogam", 540},  // xx-Ogam
  {"xx-Olck", 580},  // xx-Olck
  {"xx-Orkh", 602},  // xx-Orkh
  {"xx-Orya", 525},  // xx-Orya
  {"xx-Osma", 562},  // xx-Osma
  {"xx-Phag", 576},  // xx-Phag
  {"xx-Phli", 601},  // xx-Phli
  {"xx-Phnx", 575},  // xx-Phnx
  {"xx-Plrd", 610},  // xx-Plrd
  {"xx-Prti", 600},  // xx-Prti
  {"xx-Qaai", 552},  // xx-Qaai
  {"xx-Rjng", 584},  // xx-Rjng
  {"xx-Runr", 541},  // xx-Runr
  {"xx-Samr", 593},  // xx-Samr
  {"xx-Sarb", 599},  // xx-Sarb
  {"xx-Saur", 582},  // xx-Saur
  {"xx-Shaw", 561},  // xx-Shaw
  {"xx-Shrd", 611},  // xx-Shrd
  {"xx-Sinh", 530},  // xx-Sinh
  {"xx-Sora", 612},  // xx-Sora
  {"xx-Sund", 578},  // xx-Sund
  {"xx-Sylo", 570},  // xx-Sylo
  {"xx-Syrc", 519},  // xx-Syrc
  {"xx-Tagb", 556},  // xx-Tagb
  {"xx-Takr", 613},  // xx-Takr
  {"xx-Tale", 558},  // xx-Tale
  {"xx-Talu", 567},  // xx-Talu
  {"xx-Taml", 526},  // xx-Taml
  {"xx-Tavt", 590},  // xx-Tavt
  {"xx-Telu", 527},  // xx-Telu
  {"xx-Tfng", 569},  // xx-Tfng
  {"xx-Tglg", 553},  // xx-Tglg
  {"xx-Thaa", 520},  // xx-Thaa
  {"xx-Thai", 531},  // xx-Thai
  {"xx-Tibt", 533},  // xx-Tibt
  {"xx-Ugar", 560},  // xx-Ugar
  {"xx-Vaii", 581},  // xx-Vaii
  {"xx-Xpeo", 571},  // xx-Xpeo
  {"xx-Xsux", 574},  // xx-Xsux
  {"xx-Yiii", 548},  // xx-Yiii
  {"xx-Zyyy", 512},  // xx-Zyyy
  {"xxx",   25},  // xxx
  {"yi",    91},  // yi
  {"yo",   123},  // yo
  {"za",   155},  // za
  {"zh",    16},  // zh
  {"zh-CN",  16},  // zh-CN
  {"zh-HK",  69},  // zh-HK
  {"zh-Hani",  16},  // zh-Hani
  {"zh-Hans",  16},  // zh-Hans
  {"zh-Hant",  69},  // zh-Hant
  {"zh-SG",  69},  // zh-SG
  {"zh-TW",  69},  // zh-TW
  {"zhT",   69},  // zhT
  {"zht",   69},  // zht
  {"zu",    84},  // zu
  {"zzb",  507},  // zzb
  {"zze",  511},  // zze
  {"zzh",  509},  // zzh
  {"zzp",  508},  // zzp
};

}  // namespace CLD2
