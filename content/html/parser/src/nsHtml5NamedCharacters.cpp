#define nsHtml5NamedCharacters_cpp__
#include "prtypes.h"
#include "jArray.h"
#include "nscore.h"

#include "nsHtml5NamedCharacters.h"

jArray<jArray<PRUnichar,PRInt32>,PRInt32> nsHtml5NamedCharacters::NAMES = jArray<jArray<PRUnichar,PRInt32>,PRInt32>();
jArray<PRUnichar,PRInt32>* nsHtml5NamedCharacters::VALUES = nsnull;
PRUnichar** nsHtml5NamedCharacters::WINDOWS_1252 = nsnull;
PRUnichar nsHtml5NamedCharacters::WINDOWS_1252_DATA[] = {
  0x20AC,
  0xFFFD,
  0x201A,
  0x0192,
  0x201E,
  0x2026,
  0x2020,
  0x2021,
  0x02C6,
  0x2030,
  0x0160,
  0x2039,
  0x0152,
  0xFFFD,
  0x017D,
  0xFFFD,
  0xFFFD,
  0x2018,
  0x2019,
  0x201C,
  0x201D,
  0x2022,
  0x2013,
  0x2014,
  0x02DC,
  0x2122,
  0x0161,
  0x203A,
  0x0153,
  0xFFFD,
  0x017E,
  0x0178
};
PRUnichar nsHtml5NamedCharacters::NAME_0[] = {
  'A', 'E', 'l', 'i', 'g'
};
PRUnichar nsHtml5NamedCharacters::VALUE_0[] = {
  0x00c6
};
PRUnichar nsHtml5NamedCharacters::NAME_1[] = {
  'A', 'E', 'l', 'i', 'g', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1[] = {
  0x00c6
};
PRUnichar nsHtml5NamedCharacters::NAME_2[] = {
  'A', 'M', 'P'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2[] = {
  0x0026
};
PRUnichar nsHtml5NamedCharacters::NAME_3[] = {
  'A', 'M', 'P', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_3[] = {
  0x0026
};
PRUnichar nsHtml5NamedCharacters::NAME_4[] = {
  'A', 'a', 'c', 'u', 't', 'e'
};
PRUnichar nsHtml5NamedCharacters::VALUE_4[] = {
  0x00c1
};
PRUnichar nsHtml5NamedCharacters::NAME_5[] = {
  'A', 'a', 'c', 'u', 't', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_5[] = {
  0x00c1
};
PRUnichar nsHtml5NamedCharacters::NAME_6[] = {
  'A', 'b', 'r', 'e', 'v', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_6[] = {
  0x0102
};
PRUnichar nsHtml5NamedCharacters::NAME_7[] = {
  'A', 'c', 'i', 'r', 'c'
};
PRUnichar nsHtml5NamedCharacters::VALUE_7[] = {
  0x00c2
};
PRUnichar nsHtml5NamedCharacters::NAME_8[] = {
  'A', 'c', 'i', 'r', 'c', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_8[] = {
  0x00c2
};
PRUnichar nsHtml5NamedCharacters::NAME_9[] = {
  'A', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_9[] = {
  0x0410
};
PRUnichar nsHtml5NamedCharacters::NAME_10[] = {
  'A', 'f', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_10[] = {
  0xd835, 0xdd04
};
PRUnichar nsHtml5NamedCharacters::NAME_11[] = {
  'A', 'g', 'r', 'a', 'v', 'e'
};
PRUnichar nsHtml5NamedCharacters::VALUE_11[] = {
  0x00c0
};
PRUnichar nsHtml5NamedCharacters::NAME_12[] = {
  'A', 'g', 'r', 'a', 'v', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_12[] = {
  0x00c0
};
PRUnichar nsHtml5NamedCharacters::NAME_13[] = {
  'A', 'l', 'p', 'h', 'a', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_13[] = {
  0x0391
};
PRUnichar nsHtml5NamedCharacters::NAME_14[] = {
  'A', 'm', 'a', 'c', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_14[] = {
  0x0100
};
PRUnichar nsHtml5NamedCharacters::NAME_15[] = {
  'A', 'n', 'd', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_15[] = {
  0x2a53
};
PRUnichar nsHtml5NamedCharacters::NAME_16[] = {
  'A', 'o', 'g', 'o', 'n', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_16[] = {
  0x0104
};
PRUnichar nsHtml5NamedCharacters::NAME_17[] = {
  'A', 'o', 'p', 'f', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_17[] = {
  0xd835, 0xdd38
};
PRUnichar nsHtml5NamedCharacters::NAME_18[] = {
  'A', 'p', 'p', 'l', 'y', 'F', 'u', 'n', 'c', 't', 'i', 'o', 'n', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_18[] = {
  0x2061
};
PRUnichar nsHtml5NamedCharacters::NAME_19[] = {
  'A', 'r', 'i', 'n', 'g'
};
PRUnichar nsHtml5NamedCharacters::VALUE_19[] = {
  0x00c5
};
PRUnichar nsHtml5NamedCharacters::NAME_20[] = {
  'A', 'r', 'i', 'n', 'g', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_20[] = {
  0x00c5
};
PRUnichar nsHtml5NamedCharacters::NAME_21[] = {
  'A', 's', 'c', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_21[] = {
  0xd835, 0xdc9c
};
PRUnichar nsHtml5NamedCharacters::NAME_22[] = {
  'A', 's', 's', 'i', 'g', 'n', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_22[] = {
  0x2254
};
PRUnichar nsHtml5NamedCharacters::NAME_23[] = {
  'A', 't', 'i', 'l', 'd', 'e'
};
PRUnichar nsHtml5NamedCharacters::VALUE_23[] = {
  0x00c3
};
PRUnichar nsHtml5NamedCharacters::NAME_24[] = {
  'A', 't', 'i', 'l', 'd', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_24[] = {
  0x00c3
};
PRUnichar nsHtml5NamedCharacters::NAME_25[] = {
  'A', 'u', 'm', 'l'
};
PRUnichar nsHtml5NamedCharacters::VALUE_25[] = {
  0x00c4
};
PRUnichar nsHtml5NamedCharacters::NAME_26[] = {
  'A', 'u', 'm', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_26[] = {
  0x00c4
};
PRUnichar nsHtml5NamedCharacters::NAME_27[] = {
  'B', 'a', 'c', 'k', 's', 'l', 'a', 's', 'h', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_27[] = {
  0x2216
};
PRUnichar nsHtml5NamedCharacters::NAME_28[] = {
  'B', 'a', 'r', 'v', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_28[] = {
  0x2ae7
};
PRUnichar nsHtml5NamedCharacters::NAME_29[] = {
  'B', 'a', 'r', 'w', 'e', 'd', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_29[] = {
  0x2306
};
PRUnichar nsHtml5NamedCharacters::NAME_30[] = {
  'B', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_30[] = {
  0x0411
};
PRUnichar nsHtml5NamedCharacters::NAME_31[] = {
  'B', 'e', 'c', 'a', 'u', 's', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_31[] = {
  0x2235
};
PRUnichar nsHtml5NamedCharacters::NAME_32[] = {
  'B', 'e', 'r', 'n', 'o', 'u', 'l', 'l', 'i', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_32[] = {
  0x212c
};
PRUnichar nsHtml5NamedCharacters::NAME_33[] = {
  'B', 'e', 't', 'a', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_33[] = {
  0x0392
};
PRUnichar nsHtml5NamedCharacters::NAME_34[] = {
  'B', 'f', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_34[] = {
  0xd835, 0xdd05
};
PRUnichar nsHtml5NamedCharacters::NAME_35[] = {
  'B', 'o', 'p', 'f', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_35[] = {
  0xd835, 0xdd39
};
PRUnichar nsHtml5NamedCharacters::NAME_36[] = {
  'B', 'r', 'e', 'v', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_36[] = {
  0x02d8
};
PRUnichar nsHtml5NamedCharacters::NAME_37[] = {
  'B', 's', 'c', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_37[] = {
  0x212c
};
PRUnichar nsHtml5NamedCharacters::NAME_38[] = {
  'B', 'u', 'm', 'p', 'e', 'q', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_38[] = {
  0x224e
};
PRUnichar nsHtml5NamedCharacters::NAME_39[] = {
  'C', 'H', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_39[] = {
  0x0427
};
PRUnichar nsHtml5NamedCharacters::NAME_40[] = {
  'C', 'O', 'P', 'Y'
};
PRUnichar nsHtml5NamedCharacters::VALUE_40[] = {
  0x00a9
};
PRUnichar nsHtml5NamedCharacters::NAME_41[] = {
  'C', 'O', 'P', 'Y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_41[] = {
  0x00a9
};
PRUnichar nsHtml5NamedCharacters::NAME_42[] = {
  'C', 'a', 'c', 'u', 't', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_42[] = {
  0x0106
};
PRUnichar nsHtml5NamedCharacters::NAME_43[] = {
  'C', 'a', 'p', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_43[] = {
  0x22d2
};
PRUnichar nsHtml5NamedCharacters::NAME_44[] = {
  'C', 'a', 'p', 'i', 't', 'a', 'l', 'D', 'i', 'f', 'f', 'e', 'r', 'e', 'n', 't', 'i', 'a', 'l', 'D', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_44[] = {
  0x2145
};
PRUnichar nsHtml5NamedCharacters::NAME_45[] = {
  'C', 'a', 'y', 'l', 'e', 'y', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_45[] = {
  0x212d
};
PRUnichar nsHtml5NamedCharacters::NAME_46[] = {
  'C', 'c', 'a', 'r', 'o', 'n', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_46[] = {
  0x010c
};
PRUnichar nsHtml5NamedCharacters::NAME_47[] = {
  'C', 'c', 'e', 'd', 'i', 'l'
};
PRUnichar nsHtml5NamedCharacters::VALUE_47[] = {
  0x00c7
};
PRUnichar nsHtml5NamedCharacters::NAME_48[] = {
  'C', 'c', 'e', 'd', 'i', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_48[] = {
  0x00c7
};
PRUnichar nsHtml5NamedCharacters::NAME_49[] = {
  'C', 'c', 'i', 'r', 'c', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_49[] = {
  0x0108
};
PRUnichar nsHtml5NamedCharacters::NAME_50[] = {
  'C', 'c', 'o', 'n', 'i', 'n', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_50[] = {
  0x2230
};
PRUnichar nsHtml5NamedCharacters::NAME_51[] = {
  'C', 'd', 'o', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_51[] = {
  0x010a
};
PRUnichar nsHtml5NamedCharacters::NAME_52[] = {
  'C', 'e', 'd', 'i', 'l', 'l', 'a', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_52[] = {
  0x00b8
};
PRUnichar nsHtml5NamedCharacters::NAME_53[] = {
  'C', 'e', 'n', 't', 'e', 'r', 'D', 'o', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_53[] = {
  0x00b7
};
PRUnichar nsHtml5NamedCharacters::NAME_54[] = {
  'C', 'f', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_54[] = {
  0x212d
};
PRUnichar nsHtml5NamedCharacters::NAME_55[] = {
  'C', 'h', 'i', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_55[] = {
  0x03a7
};
PRUnichar nsHtml5NamedCharacters::NAME_56[] = {
  'C', 'i', 'r', 'c', 'l', 'e', 'D', 'o', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_56[] = {
  0x2299
};
PRUnichar nsHtml5NamedCharacters::NAME_57[] = {
  'C', 'i', 'r', 'c', 'l', 'e', 'M', 'i', 'n', 'u', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_57[] = {
  0x2296
};
PRUnichar nsHtml5NamedCharacters::NAME_58[] = {
  'C', 'i', 'r', 'c', 'l', 'e', 'P', 'l', 'u', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_58[] = {
  0x2295
};
PRUnichar nsHtml5NamedCharacters::NAME_59[] = {
  'C', 'i', 'r', 'c', 'l', 'e', 'T', 'i', 'm', 'e', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_59[] = {
  0x2297
};
PRUnichar nsHtml5NamedCharacters::NAME_60[] = {
  'C', 'l', 'o', 'c', 'k', 'w', 'i', 's', 'e', 'C', 'o', 'n', 't', 'o', 'u', 'r', 'I', 'n', 't', 'e', 'g', 'r', 'a', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_60[] = {
  0x2232
};
PRUnichar nsHtml5NamedCharacters::NAME_61[] = {
  'C', 'l', 'o', 's', 'e', 'C', 'u', 'r', 'l', 'y', 'D', 'o', 'u', 'b', 'l', 'e', 'Q', 'u', 'o', 't', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_61[] = {
  0x201d
};
PRUnichar nsHtml5NamedCharacters::NAME_62[] = {
  'C', 'l', 'o', 's', 'e', 'C', 'u', 'r', 'l', 'y', 'Q', 'u', 'o', 't', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_62[] = {
  0x2019
};
PRUnichar nsHtml5NamedCharacters::NAME_63[] = {
  'C', 'o', 'l', 'o', 'n', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_63[] = {
  0x2237
};
PRUnichar nsHtml5NamedCharacters::NAME_64[] = {
  'C', 'o', 'l', 'o', 'n', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_64[] = {
  0x2a74
};
PRUnichar nsHtml5NamedCharacters::NAME_65[] = {
  'C', 'o', 'n', 'g', 'r', 'u', 'e', 'n', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_65[] = {
  0x2261
};
PRUnichar nsHtml5NamedCharacters::NAME_66[] = {
  'C', 'o', 'n', 'i', 'n', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_66[] = {
  0x222f
};
PRUnichar nsHtml5NamedCharacters::NAME_67[] = {
  'C', 'o', 'n', 't', 'o', 'u', 'r', 'I', 'n', 't', 'e', 'g', 'r', 'a', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_67[] = {
  0x222e
};
PRUnichar nsHtml5NamedCharacters::NAME_68[] = {
  'C', 'o', 'p', 'f', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_68[] = {
  0x2102
};
PRUnichar nsHtml5NamedCharacters::NAME_69[] = {
  'C', 'o', 'p', 'r', 'o', 'd', 'u', 'c', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_69[] = {
  0x2210
};
PRUnichar nsHtml5NamedCharacters::NAME_70[] = {
  'C', 'o', 'u', 'n', 't', 'e', 'r', 'C', 'l', 'o', 'c', 'k', 'w', 'i', 's', 'e', 'C', 'o', 'n', 't', 'o', 'u', 'r', 'I', 'n', 't', 'e', 'g', 'r', 'a', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_70[] = {
  0x2233
};
PRUnichar nsHtml5NamedCharacters::NAME_71[] = {
  'C', 'r', 'o', 's', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_71[] = {
  0x2a2f
};
PRUnichar nsHtml5NamedCharacters::NAME_72[] = {
  'C', 's', 'c', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_72[] = {
  0xd835, 0xdc9e
};
PRUnichar nsHtml5NamedCharacters::NAME_73[] = {
  'C', 'u', 'p', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_73[] = {
  0x22d3
};
PRUnichar nsHtml5NamedCharacters::NAME_74[] = {
  'C', 'u', 'p', 'C', 'a', 'p', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_74[] = {
  0x224d
};
PRUnichar nsHtml5NamedCharacters::NAME_75[] = {
  'D', 'D', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_75[] = {
  0x2145
};
PRUnichar nsHtml5NamedCharacters::NAME_76[] = {
  'D', 'D', 'o', 't', 'r', 'a', 'h', 'd', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_76[] = {
  0x2911
};
PRUnichar nsHtml5NamedCharacters::NAME_77[] = {
  'D', 'J', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_77[] = {
  0x0402
};
PRUnichar nsHtml5NamedCharacters::NAME_78[] = {
  'D', 'S', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_78[] = {
  0x0405
};
PRUnichar nsHtml5NamedCharacters::NAME_79[] = {
  'D', 'Z', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_79[] = {
  0x040f
};
PRUnichar nsHtml5NamedCharacters::NAME_80[] = {
  'D', 'a', 'g', 'g', 'e', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_80[] = {
  0x2021
};
PRUnichar nsHtml5NamedCharacters::NAME_81[] = {
  'D', 'a', 'r', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_81[] = {
  0x21a1
};
PRUnichar nsHtml5NamedCharacters::NAME_82[] = {
  'D', 'a', 's', 'h', 'v', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_82[] = {
  0x2ae4
};
PRUnichar nsHtml5NamedCharacters::NAME_83[] = {
  'D', 'c', 'a', 'r', 'o', 'n', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_83[] = {
  0x010e
};
PRUnichar nsHtml5NamedCharacters::NAME_84[] = {
  'D', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_84[] = {
  0x0414
};
PRUnichar nsHtml5NamedCharacters::NAME_85[] = {
  'D', 'e', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_85[] = {
  0x2207
};
PRUnichar nsHtml5NamedCharacters::NAME_86[] = {
  'D', 'e', 'l', 't', 'a', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_86[] = {
  0x0394
};
PRUnichar nsHtml5NamedCharacters::NAME_87[] = {
  'D', 'f', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_87[] = {
  0xd835, 0xdd07
};
PRUnichar nsHtml5NamedCharacters::NAME_88[] = {
  'D', 'i', 'a', 'c', 'r', 'i', 't', 'i', 'c', 'a', 'l', 'A', 'c', 'u', 't', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_88[] = {
  0x00b4
};
PRUnichar nsHtml5NamedCharacters::NAME_89[] = {
  'D', 'i', 'a', 'c', 'r', 'i', 't', 'i', 'c', 'a', 'l', 'D', 'o', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_89[] = {
  0x02d9
};
PRUnichar nsHtml5NamedCharacters::NAME_90[] = {
  'D', 'i', 'a', 'c', 'r', 'i', 't', 'i', 'c', 'a', 'l', 'D', 'o', 'u', 'b', 'l', 'e', 'A', 'c', 'u', 't', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_90[] = {
  0x02dd
};
PRUnichar nsHtml5NamedCharacters::NAME_91[] = {
  'D', 'i', 'a', 'c', 'r', 'i', 't', 'i', 'c', 'a', 'l', 'G', 'r', 'a', 'v', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_91[] = {
  0x0060
};
PRUnichar nsHtml5NamedCharacters::NAME_92[] = {
  'D', 'i', 'a', 'c', 'r', 'i', 't', 'i', 'c', 'a', 'l', 'T', 'i', 'l', 'd', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_92[] = {
  0x02dc
};
PRUnichar nsHtml5NamedCharacters::NAME_93[] = {
  'D', 'i', 'a', 'm', 'o', 'n', 'd', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_93[] = {
  0x22c4
};
PRUnichar nsHtml5NamedCharacters::NAME_94[] = {
  'D', 'i', 'f', 'f', 'e', 'r', 'e', 'n', 't', 'i', 'a', 'l', 'D', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_94[] = {
  0x2146
};
PRUnichar nsHtml5NamedCharacters::NAME_95[] = {
  'D', 'o', 'p', 'f', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_95[] = {
  0xd835, 0xdd3b
};
PRUnichar nsHtml5NamedCharacters::NAME_96[] = {
  'D', 'o', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_96[] = {
  0x00a8
};
PRUnichar nsHtml5NamedCharacters::NAME_97[] = {
  'D', 'o', 't', 'D', 'o', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_97[] = {
  0x20dc
};
PRUnichar nsHtml5NamedCharacters::NAME_98[] = {
  'D', 'o', 't', 'E', 'q', 'u', 'a', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_98[] = {
  0x2250
};
PRUnichar nsHtml5NamedCharacters::NAME_99[] = {
  'D', 'o', 'u', 'b', 'l', 'e', 'C', 'o', 'n', 't', 'o', 'u', 'r', 'I', 'n', 't', 'e', 'g', 'r', 'a', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_99[] = {
  0x222f
};
PRUnichar nsHtml5NamedCharacters::NAME_100[] = {
  'D', 'o', 'u', 'b', 'l', 'e', 'D', 'o', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_100[] = {
  0x00a8
};
PRUnichar nsHtml5NamedCharacters::NAME_101[] = {
  'D', 'o', 'u', 'b', 'l', 'e', 'D', 'o', 'w', 'n', 'A', 'r', 'r', 'o', 'w', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_101[] = {
  0x21d3
};
PRUnichar nsHtml5NamedCharacters::NAME_102[] = {
  'D', 'o', 'u', 'b', 'l', 'e', 'L', 'e', 'f', 't', 'A', 'r', 'r', 'o', 'w', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_102[] = {
  0x21d0
};
PRUnichar nsHtml5NamedCharacters::NAME_103[] = {
  'D', 'o', 'u', 'b', 'l', 'e', 'L', 'e', 'f', 't', 'R', 'i', 'g', 'h', 't', 'A', 'r', 'r', 'o', 'w', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_103[] = {
  0x21d4
};
PRUnichar nsHtml5NamedCharacters::NAME_104[] = {
  'D', 'o', 'u', 'b', 'l', 'e', 'L', 'e', 'f', 't', 'T', 'e', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_104[] = {
  0x2ae4
};
PRUnichar nsHtml5NamedCharacters::NAME_105[] = {
  'D', 'o', 'u', 'b', 'l', 'e', 'L', 'o', 'n', 'g', 'L', 'e', 'f', 't', 'A', 'r', 'r', 'o', 'w', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_105[] = {
  0x27f8
};
PRUnichar nsHtml5NamedCharacters::NAME_106[] = {
  'D', 'o', 'u', 'b', 'l', 'e', 'L', 'o', 'n', 'g', 'L', 'e', 'f', 't', 'R', 'i', 'g', 'h', 't', 'A', 'r', 'r', 'o', 'w', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_106[] = {
  0x27fa
};
PRUnichar nsHtml5NamedCharacters::NAME_107[] = {
  'D', 'o', 'u', 'b', 'l', 'e', 'L', 'o', 'n', 'g', 'R', 'i', 'g', 'h', 't', 'A', 'r', 'r', 'o', 'w', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_107[] = {
  0x27f9
};
PRUnichar nsHtml5NamedCharacters::NAME_108[] = {
  'D', 'o', 'u', 'b', 'l', 'e', 'R', 'i', 'g', 'h', 't', 'A', 'r', 'r', 'o', 'w', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_108[] = {
  0x21d2
};
PRUnichar nsHtml5NamedCharacters::NAME_109[] = {
  'D', 'o', 'u', 'b', 'l', 'e', 'R', 'i', 'g', 'h', 't', 'T', 'e', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_109[] = {
  0x22a8
};
PRUnichar nsHtml5NamedCharacters::NAME_110[] = {
  'D', 'o', 'u', 'b', 'l', 'e', 'U', 'p', 'A', 'r', 'r', 'o', 'w', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_110[] = {
  0x21d1
};
PRUnichar nsHtml5NamedCharacters::NAME_111[] = {
  'D', 'o', 'u', 'b', 'l', 'e', 'U', 'p', 'D', 'o', 'w', 'n', 'A', 'r', 'r', 'o', 'w', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_111[] = {
  0x21d5
};
PRUnichar nsHtml5NamedCharacters::NAME_112[] = {
  'D', 'o', 'u', 'b', 'l', 'e', 'V', 'e', 'r', 't', 'i', 'c', 'a', 'l', 'B', 'a', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_112[] = {
  0x2225
};
PRUnichar nsHtml5NamedCharacters::NAME_113[] = {
  'D', 'o', 'w', 'n', 'A', 'r', 'r', 'o', 'w', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_113[] = {
  0x2193
};
PRUnichar nsHtml5NamedCharacters::NAME_114[] = {
  'D', 'o', 'w', 'n', 'A', 'r', 'r', 'o', 'w', 'B', 'a', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_114[] = {
  0x2913
};
PRUnichar nsHtml5NamedCharacters::NAME_115[] = {
  'D', 'o', 'w', 'n', 'A', 'r', 'r', 'o', 'w', 'U', 'p', 'A', 'r', 'r', 'o', 'w', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_115[] = {
  0x21f5
};
PRUnichar nsHtml5NamedCharacters::NAME_116[] = {
  'D', 'o', 'w', 'n', 'B', 'r', 'e', 'v', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_116[] = {
  0x0311
};
PRUnichar nsHtml5NamedCharacters::NAME_117[] = {
  'D', 'o', 'w', 'n', 'L', 'e', 'f', 't', 'R', 'i', 'g', 'h', 't', 'V', 'e', 'c', 't', 'o', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_117[] = {
  0x2950
};
PRUnichar nsHtml5NamedCharacters::NAME_118[] = {
  'D', 'o', 'w', 'n', 'L', 'e', 'f', 't', 'T', 'e', 'e', 'V', 'e', 'c', 't', 'o', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_118[] = {
  0x295e
};
PRUnichar nsHtml5NamedCharacters::NAME_119[] = {
  'D', 'o', 'w', 'n', 'L', 'e', 'f', 't', 'V', 'e', 'c', 't', 'o', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_119[] = {
  0x21bd
};
PRUnichar nsHtml5NamedCharacters::NAME_120[] = {
  'D', 'o', 'w', 'n', 'L', 'e', 'f', 't', 'V', 'e', 'c', 't', 'o', 'r', 'B', 'a', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_120[] = {
  0x2956
};
PRUnichar nsHtml5NamedCharacters::NAME_121[] = {
  'D', 'o', 'w', 'n', 'R', 'i', 'g', 'h', 't', 'T', 'e', 'e', 'V', 'e', 'c', 't', 'o', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_121[] = {
  0x295f
};
PRUnichar nsHtml5NamedCharacters::NAME_122[] = {
  'D', 'o', 'w', 'n', 'R', 'i', 'g', 'h', 't', 'V', 'e', 'c', 't', 'o', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_122[] = {
  0x21c1
};
PRUnichar nsHtml5NamedCharacters::NAME_123[] = {
  'D', 'o', 'w', 'n', 'R', 'i', 'g', 'h', 't', 'V', 'e', 'c', 't', 'o', 'r', 'B', 'a', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_123[] = {
  0x2957
};
PRUnichar nsHtml5NamedCharacters::NAME_124[] = {
  'D', 'o', 'w', 'n', 'T', 'e', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_124[] = {
  0x22a4
};
PRUnichar nsHtml5NamedCharacters::NAME_125[] = {
  'D', 'o', 'w', 'n', 'T', 'e', 'e', 'A', 'r', 'r', 'o', 'w', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_125[] = {
  0x21a7
};
PRUnichar nsHtml5NamedCharacters::NAME_126[] = {
  'D', 'o', 'w', 'n', 'a', 'r', 'r', 'o', 'w', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_126[] = {
  0x21d3
};
PRUnichar nsHtml5NamedCharacters::NAME_127[] = {
  'D', 's', 'c', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_127[] = {
  0xd835, 0xdc9f
};
PRUnichar nsHtml5NamedCharacters::NAME_128[] = {
  'D', 's', 't', 'r', 'o', 'k', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_128[] = {
  0x0110
};
PRUnichar nsHtml5NamedCharacters::NAME_129[] = {
  'E', 'N', 'G', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_129[] = {
  0x014a
};
PRUnichar nsHtml5NamedCharacters::NAME_130[] = {
  'E', 'T', 'H'
};
PRUnichar nsHtml5NamedCharacters::VALUE_130[] = {
  0x00d0
};
PRUnichar nsHtml5NamedCharacters::NAME_131[] = {
  'E', 'T', 'H', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_131[] = {
  0x00d0
};
PRUnichar nsHtml5NamedCharacters::NAME_132[] = {
  'E', 'a', 'c', 'u', 't', 'e'
};
PRUnichar nsHtml5NamedCharacters::VALUE_132[] = {
  0x00c9
};
PRUnichar nsHtml5NamedCharacters::NAME_133[] = {
  'E', 'a', 'c', 'u', 't', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_133[] = {
  0x00c9
};
PRUnichar nsHtml5NamedCharacters::NAME_134[] = {
  'E', 'c', 'a', 'r', 'o', 'n', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_134[] = {
  0x011a
};
PRUnichar nsHtml5NamedCharacters::NAME_135[] = {
  'E', 'c', 'i', 'r', 'c'
};
PRUnichar nsHtml5NamedCharacters::VALUE_135[] = {
  0x00ca
};
PRUnichar nsHtml5NamedCharacters::NAME_136[] = {
  'E', 'c', 'i', 'r', 'c', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_136[] = {
  0x00ca
};
PRUnichar nsHtml5NamedCharacters::NAME_137[] = {
  'E', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_137[] = {
  0x042d
};
PRUnichar nsHtml5NamedCharacters::NAME_138[] = {
  'E', 'd', 'o', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_138[] = {
  0x0116
};
PRUnichar nsHtml5NamedCharacters::NAME_139[] = {
  'E', 'f', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_139[] = {
  0xd835, 0xdd08
};
PRUnichar nsHtml5NamedCharacters::NAME_140[] = {
  'E', 'g', 'r', 'a', 'v', 'e'
};
PRUnichar nsHtml5NamedCharacters::VALUE_140[] = {
  0x00c8
};
PRUnichar nsHtml5NamedCharacters::NAME_141[] = {
  'E', 'g', 'r', 'a', 'v', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_141[] = {
  0x00c8
};
PRUnichar nsHtml5NamedCharacters::NAME_142[] = {
  'E', 'l', 'e', 'm', 'e', 'n', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_142[] = {
  0x2208
};
PRUnichar nsHtml5NamedCharacters::NAME_143[] = {
  'E', 'm', 'a', 'c', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_143[] = {
  0x0112
};
PRUnichar nsHtml5NamedCharacters::NAME_144[] = {
  'E', 'm', 'p', 't', 'y', 'S', 'm', 'a', 'l', 'l', 'S', 'q', 'u', 'a', 'r', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_144[] = {
  0x25fb
};
PRUnichar nsHtml5NamedCharacters::NAME_145[] = {
  'E', 'm', 'p', 't', 'y', 'V', 'e', 'r', 'y', 'S', 'm', 'a', 'l', 'l', 'S', 'q', 'u', 'a', 'r', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_145[] = {
  0x25ab
};
PRUnichar nsHtml5NamedCharacters::NAME_146[] = {
  'E', 'o', 'g', 'o', 'n', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_146[] = {
  0x0118
};
PRUnichar nsHtml5NamedCharacters::NAME_147[] = {
  'E', 'o', 'p', 'f', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_147[] = {
  0xd835, 0xdd3c
};
PRUnichar nsHtml5NamedCharacters::NAME_148[] = {
  'E', 'p', 's', 'i', 'l', 'o', 'n', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_148[] = {
  0x0395
};
PRUnichar nsHtml5NamedCharacters::NAME_149[] = {
  'E', 'q', 'u', 'a', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_149[] = {
  0x2a75
};
PRUnichar nsHtml5NamedCharacters::NAME_150[] = {
  'E', 'q', 'u', 'a', 'l', 'T', 'i', 'l', 'd', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_150[] = {
  0x2242
};
PRUnichar nsHtml5NamedCharacters::NAME_151[] = {
  'E', 'q', 'u', 'i', 'l', 'i', 'b', 'r', 'i', 'u', 'm', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_151[] = {
  0x21cc
};
PRUnichar nsHtml5NamedCharacters::NAME_152[] = {
  'E', 's', 'c', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_152[] = {
  0x2130
};
PRUnichar nsHtml5NamedCharacters::NAME_153[] = {
  'E', 's', 'i', 'm', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_153[] = {
  0x2a73
};
PRUnichar nsHtml5NamedCharacters::NAME_154[] = {
  'E', 't', 'a', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_154[] = {
  0x0397
};
PRUnichar nsHtml5NamedCharacters::NAME_155[] = {
  'E', 'u', 'm', 'l'
};
PRUnichar nsHtml5NamedCharacters::VALUE_155[] = {
  0x00cb
};
PRUnichar nsHtml5NamedCharacters::NAME_156[] = {
  'E', 'u', 'm', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_156[] = {
  0x00cb
};
PRUnichar nsHtml5NamedCharacters::NAME_157[] = {
  'E', 'x', 'i', 's', 't', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_157[] = {
  0x2203
};
PRUnichar nsHtml5NamedCharacters::NAME_158[] = {
  'E', 'x', 'p', 'o', 'n', 'e', 'n', 't', 'i', 'a', 'l', 'E', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_158[] = {
  0x2147
};
PRUnichar nsHtml5NamedCharacters::NAME_159[] = {
  'F', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_159[] = {
  0x0424
};
PRUnichar nsHtml5NamedCharacters::NAME_160[] = {
  'F', 'f', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_160[] = {
  0xd835, 0xdd09
};
PRUnichar nsHtml5NamedCharacters::NAME_161[] = {
  'F', 'i', 'l', 'l', 'e', 'd', 'S', 'm', 'a', 'l', 'l', 'S', 'q', 'u', 'a', 'r', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_161[] = {
  0x25fc
};
PRUnichar nsHtml5NamedCharacters::NAME_162[] = {
  'F', 'i', 'l', 'l', 'e', 'd', 'V', 'e', 'r', 'y', 'S', 'm', 'a', 'l', 'l', 'S', 'q', 'u', 'a', 'r', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_162[] = {
  0x25aa
};
PRUnichar nsHtml5NamedCharacters::NAME_163[] = {
  'F', 'o', 'p', 'f', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_163[] = {
  0xd835, 0xdd3d
};
PRUnichar nsHtml5NamedCharacters::NAME_164[] = {
  'F', 'o', 'r', 'A', 'l', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_164[] = {
  0x2200
};
PRUnichar nsHtml5NamedCharacters::NAME_165[] = {
  'F', 'o', 'u', 'r', 'i', 'e', 'r', 't', 'r', 'f', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_165[] = {
  0x2131
};
PRUnichar nsHtml5NamedCharacters::NAME_166[] = {
  'F', 's', 'c', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_166[] = {
  0x2131
};
PRUnichar nsHtml5NamedCharacters::NAME_167[] = {
  'G', 'J', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_167[] = {
  0x0403
};
PRUnichar nsHtml5NamedCharacters::NAME_168[] = {
  'G', 'T'
};
PRUnichar nsHtml5NamedCharacters::VALUE_168[] = {
  0x003e
};
PRUnichar nsHtml5NamedCharacters::NAME_169[] = {
  'G', 'T', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_169[] = {
  0x003e
};
PRUnichar nsHtml5NamedCharacters::NAME_170[] = {
  'G', 'a', 'm', 'm', 'a', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_170[] = {
  0x0393
};
PRUnichar nsHtml5NamedCharacters::NAME_171[] = {
  'G', 'a', 'm', 'm', 'a', 'd', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_171[] = {
  0x03dc
};
PRUnichar nsHtml5NamedCharacters::NAME_172[] = {
  'G', 'b', 'r', 'e', 'v', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_172[] = {
  0x011e
};
PRUnichar nsHtml5NamedCharacters::NAME_173[] = {
  'G', 'c', 'e', 'd', 'i', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_173[] = {
  0x0122
};
PRUnichar nsHtml5NamedCharacters::NAME_174[] = {
  'G', 'c', 'i', 'r', 'c', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_174[] = {
  0x011c
};
PRUnichar nsHtml5NamedCharacters::NAME_175[] = {
  'G', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_175[] = {
  0x0413
};
PRUnichar nsHtml5NamedCharacters::NAME_176[] = {
  'G', 'd', 'o', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_176[] = {
  0x0120
};
PRUnichar nsHtml5NamedCharacters::NAME_177[] = {
  'G', 'f', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_177[] = {
  0xd835, 0xdd0a
};
PRUnichar nsHtml5NamedCharacters::NAME_178[] = {
  'G', 'g', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_178[] = {
  0x22d9
};
PRUnichar nsHtml5NamedCharacters::NAME_179[] = {
  'G', 'o', 'p', 'f', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_179[] = {
  0xd835, 0xdd3e
};
PRUnichar nsHtml5NamedCharacters::NAME_180[] = {
  'G', 'r', 'e', 'a', 't', 'e', 'r', 'E', 'q', 'u', 'a', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_180[] = {
  0x2265
};
PRUnichar nsHtml5NamedCharacters::NAME_181[] = {
  'G', 'r', 'e', 'a', 't', 'e', 'r', 'E', 'q', 'u', 'a', 'l', 'L', 'e', 's', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_181[] = {
  0x22db
};
PRUnichar nsHtml5NamedCharacters::NAME_182[] = {
  'G', 'r', 'e', 'a', 't', 'e', 'r', 'F', 'u', 'l', 'l', 'E', 'q', 'u', 'a', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_182[] = {
  0x2267
};
PRUnichar nsHtml5NamedCharacters::NAME_183[] = {
  'G', 'r', 'e', 'a', 't', 'e', 'r', 'G', 'r', 'e', 'a', 't', 'e', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_183[] = {
  0x2aa2
};
PRUnichar nsHtml5NamedCharacters::NAME_184[] = {
  'G', 'r', 'e', 'a', 't', 'e', 'r', 'L', 'e', 's', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_184[] = {
  0x2277
};
PRUnichar nsHtml5NamedCharacters::NAME_185[] = {
  'G', 'r', 'e', 'a', 't', 'e', 'r', 'S', 'l', 'a', 'n', 't', 'E', 'q', 'u', 'a', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_185[] = {
  0x2a7e
};
PRUnichar nsHtml5NamedCharacters::NAME_186[] = {
  'G', 'r', 'e', 'a', 't', 'e', 'r', 'T', 'i', 'l', 'd', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_186[] = {
  0x2273
};
PRUnichar nsHtml5NamedCharacters::NAME_187[] = {
  'G', 's', 'c', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_187[] = {
  0xd835, 0xdca2
};
PRUnichar nsHtml5NamedCharacters::NAME_188[] = {
  'G', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_188[] = {
  0x226b
};
PRUnichar nsHtml5NamedCharacters::NAME_189[] = {
  'H', 'A', 'R', 'D', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_189[] = {
  0x042a
};
PRUnichar nsHtml5NamedCharacters::NAME_190[] = {
  'H', 'a', 'c', 'e', 'k', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_190[] = {
  0x02c7
};
PRUnichar nsHtml5NamedCharacters::NAME_191[] = {
  'H', 'a', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_191[] = {
  0x005e
};
PRUnichar nsHtml5NamedCharacters::NAME_192[] = {
  'H', 'c', 'i', 'r', 'c', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_192[] = {
  0x0124
};
PRUnichar nsHtml5NamedCharacters::NAME_193[] = {
  'H', 'f', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_193[] = {
  0x210c
};
PRUnichar nsHtml5NamedCharacters::NAME_194[] = {
  'H', 'i', 'l', 'b', 'e', 'r', 't', 'S', 'p', 'a', 'c', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_194[] = {
  0x210b
};
PRUnichar nsHtml5NamedCharacters::NAME_195[] = {
  'H', 'o', 'p', 'f', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_195[] = {
  0x210d
};
PRUnichar nsHtml5NamedCharacters::NAME_196[] = {
  'H', 'o', 'r', 'i', 'z', 'o', 'n', 't', 'a', 'l', 'L', 'i', 'n', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_196[] = {
  0x2500
};
PRUnichar nsHtml5NamedCharacters::NAME_197[] = {
  'H', 's', 'c', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_197[] = {
  0x210b
};
PRUnichar nsHtml5NamedCharacters::NAME_198[] = {
  'H', 's', 't', 'r', 'o', 'k', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_198[] = {
  0x0126
};
PRUnichar nsHtml5NamedCharacters::NAME_199[] = {
  'H', 'u', 'm', 'p', 'D', 'o', 'w', 'n', 'H', 'u', 'm', 'p', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_199[] = {
  0x224e
};
PRUnichar nsHtml5NamedCharacters::NAME_200[] = {
  'H', 'u', 'm', 'p', 'E', 'q', 'u', 'a', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_200[] = {
  0x224f
};
PRUnichar nsHtml5NamedCharacters::NAME_201[] = {
  'I', 'E', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_201[] = {
  0x0415
};
PRUnichar nsHtml5NamedCharacters::NAME_202[] = {
  'I', 'J', 'l', 'i', 'g', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_202[] = {
  0x0132
};
PRUnichar nsHtml5NamedCharacters::NAME_203[] = {
  'I', 'O', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_203[] = {
  0x0401
};
PRUnichar nsHtml5NamedCharacters::NAME_204[] = {
  'I', 'a', 'c', 'u', 't', 'e'
};
PRUnichar nsHtml5NamedCharacters::VALUE_204[] = {
  0x00cd
};
PRUnichar nsHtml5NamedCharacters::NAME_205[] = {
  'I', 'a', 'c', 'u', 't', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_205[] = {
  0x00cd
};
PRUnichar nsHtml5NamedCharacters::NAME_206[] = {
  'I', 'c', 'i', 'r', 'c'
};
PRUnichar nsHtml5NamedCharacters::VALUE_206[] = {
  0x00ce
};
PRUnichar nsHtml5NamedCharacters::NAME_207[] = {
  'I', 'c', 'i', 'r', 'c', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_207[] = {
  0x00ce
};
PRUnichar nsHtml5NamedCharacters::NAME_208[] = {
  'I', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_208[] = {
  0x0418
};
PRUnichar nsHtml5NamedCharacters::NAME_209[] = {
  'I', 'd', 'o', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_209[] = {
  0x0130
};
PRUnichar nsHtml5NamedCharacters::NAME_210[] = {
  'I', 'f', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_210[] = {
  0x2111
};
PRUnichar nsHtml5NamedCharacters::NAME_211[] = {
  'I', 'g', 'r', 'a', 'v', 'e'
};
PRUnichar nsHtml5NamedCharacters::VALUE_211[] = {
  0x00cc
};
PRUnichar nsHtml5NamedCharacters::NAME_212[] = {
  'I', 'g', 'r', 'a', 'v', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_212[] = {
  0x00cc
};
PRUnichar nsHtml5NamedCharacters::NAME_213[] = {
  'I', 'm', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_213[] = {
  0x2111
};
PRUnichar nsHtml5NamedCharacters::NAME_214[] = {
  'I', 'm', 'a', 'c', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_214[] = {
  0x012a
};
PRUnichar nsHtml5NamedCharacters::NAME_215[] = {
  'I', 'm', 'a', 'g', 'i', 'n', 'a', 'r', 'y', 'I', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_215[] = {
  0x2148
};
PRUnichar nsHtml5NamedCharacters::NAME_216[] = {
  'I', 'm', 'p', 'l', 'i', 'e', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_216[] = {
  0x21d2
};
PRUnichar nsHtml5NamedCharacters::NAME_217[] = {
  'I', 'n', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_217[] = {
  0x222c
};
PRUnichar nsHtml5NamedCharacters::NAME_218[] = {
  'I', 'n', 't', 'e', 'g', 'r', 'a', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_218[] = {
  0x222b
};
PRUnichar nsHtml5NamedCharacters::NAME_219[] = {
  'I', 'n', 't', 'e', 'r', 's', 'e', 'c', 't', 'i', 'o', 'n', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_219[] = {
  0x22c2
};
PRUnichar nsHtml5NamedCharacters::NAME_220[] = {
  'I', 'n', 'v', 'i', 's', 'i', 'b', 'l', 'e', 'C', 'o', 'm', 'm', 'a', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_220[] = {
  0x2063
};
PRUnichar nsHtml5NamedCharacters::NAME_221[] = {
  'I', 'n', 'v', 'i', 's', 'i', 'b', 'l', 'e', 'T', 'i', 'm', 'e', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_221[] = {
  0x2062
};
PRUnichar nsHtml5NamedCharacters::NAME_222[] = {
  'I', 'o', 'g', 'o', 'n', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_222[] = {
  0x012e
};
PRUnichar nsHtml5NamedCharacters::NAME_223[] = {
  'I', 'o', 'p', 'f', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_223[] = {
  0xd835, 0xdd40
};
PRUnichar nsHtml5NamedCharacters::NAME_224[] = {
  'I', 'o', 't', 'a', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_224[] = {
  0x0399
};
PRUnichar nsHtml5NamedCharacters::NAME_225[] = {
  'I', 's', 'c', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_225[] = {
  0x2110
};
PRUnichar nsHtml5NamedCharacters::NAME_226[] = {
  'I', 't', 'i', 'l', 'd', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_226[] = {
  0x0128
};
PRUnichar nsHtml5NamedCharacters::NAME_227[] = {
  'I', 'u', 'k', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_227[] = {
  0x0406
};
PRUnichar nsHtml5NamedCharacters::NAME_228[] = {
  'I', 'u', 'm', 'l'
};
PRUnichar nsHtml5NamedCharacters::VALUE_228[] = {
  0x00cf
};
PRUnichar nsHtml5NamedCharacters::NAME_229[] = {
  'I', 'u', 'm', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_229[] = {
  0x00cf
};
PRUnichar nsHtml5NamedCharacters::NAME_230[] = {
  'J', 'c', 'i', 'r', 'c', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_230[] = {
  0x0134
};
PRUnichar nsHtml5NamedCharacters::NAME_231[] = {
  'J', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_231[] = {
  0x0419
};
PRUnichar nsHtml5NamedCharacters::NAME_232[] = {
  'J', 'f', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_232[] = {
  0xd835, 0xdd0d
};
PRUnichar nsHtml5NamedCharacters::NAME_233[] = {
  'J', 'o', 'p', 'f', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_233[] = {
  0xd835, 0xdd41
};
PRUnichar nsHtml5NamedCharacters::NAME_234[] = {
  'J', 's', 'c', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_234[] = {
  0xd835, 0xdca5
};
PRUnichar nsHtml5NamedCharacters::NAME_235[] = {
  'J', 's', 'e', 'r', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_235[] = {
  0x0408
};
PRUnichar nsHtml5NamedCharacters::NAME_236[] = {
  'J', 'u', 'k', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_236[] = {
  0x0404
};
PRUnichar nsHtml5NamedCharacters::NAME_237[] = {
  'K', 'H', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_237[] = {
  0x0425
};
PRUnichar nsHtml5NamedCharacters::NAME_238[] = {
  'K', 'J', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_238[] = {
  0x040c
};
PRUnichar nsHtml5NamedCharacters::NAME_239[] = {
  'K', 'a', 'p', 'p', 'a', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_239[] = {
  0x039a
};
PRUnichar nsHtml5NamedCharacters::NAME_240[] = {
  'K', 'c', 'e', 'd', 'i', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_240[] = {
  0x0136
};
PRUnichar nsHtml5NamedCharacters::NAME_241[] = {
  'K', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_241[] = {
  0x041a
};
PRUnichar nsHtml5NamedCharacters::NAME_242[] = {
  'K', 'f', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_242[] = {
  0xd835, 0xdd0e
};
PRUnichar nsHtml5NamedCharacters::NAME_243[] = {
  'K', 'o', 'p', 'f', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_243[] = {
  0xd835, 0xdd42
};
PRUnichar nsHtml5NamedCharacters::NAME_244[] = {
  'K', 's', 'c', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_244[] = {
  0xd835, 0xdca6
};
PRUnichar nsHtml5NamedCharacters::NAME_245[] = {
  'L', 'J', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_245[] = {
  0x0409
};
PRUnichar nsHtml5NamedCharacters::NAME_246[] = {
  'L', 'T'
};
PRUnichar nsHtml5NamedCharacters::VALUE_246[] = {
  0x003c
};
PRUnichar nsHtml5NamedCharacters::NAME_247[] = {
  'L', 'T', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_247[] = {
  0x003c
};
PRUnichar nsHtml5NamedCharacters::NAME_248[] = {
  'L', 'a', 'c', 'u', 't', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_248[] = {
  0x0139
};
PRUnichar nsHtml5NamedCharacters::NAME_249[] = {
  'L', 'a', 'm', 'b', 'd', 'a', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_249[] = {
  0x039b
};
PRUnichar nsHtml5NamedCharacters::NAME_250[] = {
  'L', 'a', 'n', 'g', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_250[] = {
  0x27ea
};
PRUnichar nsHtml5NamedCharacters::NAME_251[] = {
  'L', 'a', 'p', 'l', 'a', 'c', 'e', 't', 'r', 'f', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_251[] = {
  0x2112
};
PRUnichar nsHtml5NamedCharacters::NAME_252[] = {
  'L', 'a', 'r', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_252[] = {
  0x219e
};
PRUnichar nsHtml5NamedCharacters::NAME_253[] = {
  'L', 'c', 'a', 'r', 'o', 'n', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_253[] = {
  0x013d
};
PRUnichar nsHtml5NamedCharacters::NAME_254[] = {
  'L', 'c', 'e', 'd', 'i', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_254[] = {
  0x013b
};
PRUnichar nsHtml5NamedCharacters::NAME_255[] = {
  'L', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_255[] = {
  0x041b
};
PRUnichar nsHtml5NamedCharacters::NAME_256[] = {
  'L', 'e', 'f', 't', 'A', 'n', 'g', 'l', 'e', 'B', 'r', 'a', 'c', 'k', 'e', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_256[] = {
  0x27e8
};
PRUnichar nsHtml5NamedCharacters::NAME_257[] = {
  'L', 'e', 'f', 't', 'A', 'r', 'r', 'o', 'w', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_257[] = {
  0x2190
};
PRUnichar nsHtml5NamedCharacters::NAME_258[] = {
  'L', 'e', 'f', 't', 'A', 'r', 'r', 'o', 'w', 'B', 'a', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_258[] = {
  0x21e4
};
PRUnichar nsHtml5NamedCharacters::NAME_259[] = {
  'L', 'e', 'f', 't', 'A', 'r', 'r', 'o', 'w', 'R', 'i', 'g', 'h', 't', 'A', 'r', 'r', 'o', 'w', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_259[] = {
  0x21c6
};
PRUnichar nsHtml5NamedCharacters::NAME_260[] = {
  'L', 'e', 'f', 't', 'C', 'e', 'i', 'l', 'i', 'n', 'g', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_260[] = {
  0x2308
};
PRUnichar nsHtml5NamedCharacters::NAME_261[] = {
  'L', 'e', 'f', 't', 'D', 'o', 'u', 'b', 'l', 'e', 'B', 'r', 'a', 'c', 'k', 'e', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_261[] = {
  0x27e6
};
PRUnichar nsHtml5NamedCharacters::NAME_262[] = {
  'L', 'e', 'f', 't', 'D', 'o', 'w', 'n', 'T', 'e', 'e', 'V', 'e', 'c', 't', 'o', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_262[] = {
  0x2961
};
PRUnichar nsHtml5NamedCharacters::NAME_263[] = {
  'L', 'e', 'f', 't', 'D', 'o', 'w', 'n', 'V', 'e', 'c', 't', 'o', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_263[] = {
  0x21c3
};
PRUnichar nsHtml5NamedCharacters::NAME_264[] = {
  'L', 'e', 'f', 't', 'D', 'o', 'w', 'n', 'V', 'e', 'c', 't', 'o', 'r', 'B', 'a', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_264[] = {
  0x2959
};
PRUnichar nsHtml5NamedCharacters::NAME_265[] = {
  'L', 'e', 'f', 't', 'F', 'l', 'o', 'o', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_265[] = {
  0x230a
};
PRUnichar nsHtml5NamedCharacters::NAME_266[] = {
  'L', 'e', 'f', 't', 'R', 'i', 'g', 'h', 't', 'A', 'r', 'r', 'o', 'w', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_266[] = {
  0x2194
};
PRUnichar nsHtml5NamedCharacters::NAME_267[] = {
  'L', 'e', 'f', 't', 'R', 'i', 'g', 'h', 't', 'V', 'e', 'c', 't', 'o', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_267[] = {
  0x294e
};
PRUnichar nsHtml5NamedCharacters::NAME_268[] = {
  'L', 'e', 'f', 't', 'T', 'e', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_268[] = {
  0x22a3
};
PRUnichar nsHtml5NamedCharacters::NAME_269[] = {
  'L', 'e', 'f', 't', 'T', 'e', 'e', 'A', 'r', 'r', 'o', 'w', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_269[] = {
  0x21a4
};
PRUnichar nsHtml5NamedCharacters::NAME_270[] = {
  'L', 'e', 'f', 't', 'T', 'e', 'e', 'V', 'e', 'c', 't', 'o', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_270[] = {
  0x295a
};
PRUnichar nsHtml5NamedCharacters::NAME_271[] = {
  'L', 'e', 'f', 't', 'T', 'r', 'i', 'a', 'n', 'g', 'l', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_271[] = {
  0x22b2
};
PRUnichar nsHtml5NamedCharacters::NAME_272[] = {
  'L', 'e', 'f', 't', 'T', 'r', 'i', 'a', 'n', 'g', 'l', 'e', 'B', 'a', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_272[] = {
  0x29cf
};
PRUnichar nsHtml5NamedCharacters::NAME_273[] = {
  'L', 'e', 'f', 't', 'T', 'r', 'i', 'a', 'n', 'g', 'l', 'e', 'E', 'q', 'u', 'a', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_273[] = {
  0x22b4
};
PRUnichar nsHtml5NamedCharacters::NAME_274[] = {
  'L', 'e', 'f', 't', 'U', 'p', 'D', 'o', 'w', 'n', 'V', 'e', 'c', 't', 'o', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_274[] = {
  0x2951
};
PRUnichar nsHtml5NamedCharacters::NAME_275[] = {
  'L', 'e', 'f', 't', 'U', 'p', 'T', 'e', 'e', 'V', 'e', 'c', 't', 'o', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_275[] = {
  0x2960
};
PRUnichar nsHtml5NamedCharacters::NAME_276[] = {
  'L', 'e', 'f', 't', 'U', 'p', 'V', 'e', 'c', 't', 'o', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_276[] = {
  0x21bf
};
PRUnichar nsHtml5NamedCharacters::NAME_277[] = {
  'L', 'e', 'f', 't', 'U', 'p', 'V', 'e', 'c', 't', 'o', 'r', 'B', 'a', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_277[] = {
  0x2958
};
PRUnichar nsHtml5NamedCharacters::NAME_278[] = {
  'L', 'e', 'f', 't', 'V', 'e', 'c', 't', 'o', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_278[] = {
  0x21bc
};
PRUnichar nsHtml5NamedCharacters::NAME_279[] = {
  'L', 'e', 'f', 't', 'V', 'e', 'c', 't', 'o', 'r', 'B', 'a', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_279[] = {
  0x2952
};
PRUnichar nsHtml5NamedCharacters::NAME_280[] = {
  'L', 'e', 'f', 't', 'a', 'r', 'r', 'o', 'w', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_280[] = {
  0x21d0
};
PRUnichar nsHtml5NamedCharacters::NAME_281[] = {
  'L', 'e', 'f', 't', 'r', 'i', 'g', 'h', 't', 'a', 'r', 'r', 'o', 'w', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_281[] = {
  0x21d4
};
PRUnichar nsHtml5NamedCharacters::NAME_282[] = {
  'L', 'e', 's', 's', 'E', 'q', 'u', 'a', 'l', 'G', 'r', 'e', 'a', 't', 'e', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_282[] = {
  0x22da
};
PRUnichar nsHtml5NamedCharacters::NAME_283[] = {
  'L', 'e', 's', 's', 'F', 'u', 'l', 'l', 'E', 'q', 'u', 'a', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_283[] = {
  0x2266
};
PRUnichar nsHtml5NamedCharacters::NAME_284[] = {
  'L', 'e', 's', 's', 'G', 'r', 'e', 'a', 't', 'e', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_284[] = {
  0x2276
};
PRUnichar nsHtml5NamedCharacters::NAME_285[] = {
  'L', 'e', 's', 's', 'L', 'e', 's', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_285[] = {
  0x2aa1
};
PRUnichar nsHtml5NamedCharacters::NAME_286[] = {
  'L', 'e', 's', 's', 'S', 'l', 'a', 'n', 't', 'E', 'q', 'u', 'a', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_286[] = {
  0x2a7d
};
PRUnichar nsHtml5NamedCharacters::NAME_287[] = {
  'L', 'e', 's', 's', 'T', 'i', 'l', 'd', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_287[] = {
  0x2272
};
PRUnichar nsHtml5NamedCharacters::NAME_288[] = {
  'L', 'f', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_288[] = {
  0xd835, 0xdd0f
};
PRUnichar nsHtml5NamedCharacters::NAME_289[] = {
  'L', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_289[] = {
  0x22d8
};
PRUnichar nsHtml5NamedCharacters::NAME_290[] = {
  'L', 'l', 'e', 'f', 't', 'a', 'r', 'r', 'o', 'w', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_290[] = {
  0x21da
};
PRUnichar nsHtml5NamedCharacters::NAME_291[] = {
  'L', 'm', 'i', 'd', 'o', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_291[] = {
  0x013f
};
PRUnichar nsHtml5NamedCharacters::NAME_292[] = {
  'L', 'o', 'n', 'g', 'L', 'e', 'f', 't', 'A', 'r', 'r', 'o', 'w', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_292[] = {
  0x27f5
};
PRUnichar nsHtml5NamedCharacters::NAME_293[] = {
  'L', 'o', 'n', 'g', 'L', 'e', 'f', 't', 'R', 'i', 'g', 'h', 't', 'A', 'r', 'r', 'o', 'w', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_293[] = {
  0x27f7
};
PRUnichar nsHtml5NamedCharacters::NAME_294[] = {
  'L', 'o', 'n', 'g', 'R', 'i', 'g', 'h', 't', 'A', 'r', 'r', 'o', 'w', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_294[] = {
  0x27f6
};
PRUnichar nsHtml5NamedCharacters::NAME_295[] = {
  'L', 'o', 'n', 'g', 'l', 'e', 'f', 't', 'a', 'r', 'r', 'o', 'w', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_295[] = {
  0x27f8
};
PRUnichar nsHtml5NamedCharacters::NAME_296[] = {
  'L', 'o', 'n', 'g', 'l', 'e', 'f', 't', 'r', 'i', 'g', 'h', 't', 'a', 'r', 'r', 'o', 'w', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_296[] = {
  0x27fa
};
PRUnichar nsHtml5NamedCharacters::NAME_297[] = {
  'L', 'o', 'n', 'g', 'r', 'i', 'g', 'h', 't', 'a', 'r', 'r', 'o', 'w', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_297[] = {
  0x27f9
};
PRUnichar nsHtml5NamedCharacters::NAME_298[] = {
  'L', 'o', 'p', 'f', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_298[] = {
  0xd835, 0xdd43
};
PRUnichar nsHtml5NamedCharacters::NAME_299[] = {
  'L', 'o', 'w', 'e', 'r', 'L', 'e', 'f', 't', 'A', 'r', 'r', 'o', 'w', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_299[] = {
  0x2199
};
PRUnichar nsHtml5NamedCharacters::NAME_300[] = {
  'L', 'o', 'w', 'e', 'r', 'R', 'i', 'g', 'h', 't', 'A', 'r', 'r', 'o', 'w', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_300[] = {
  0x2198
};
PRUnichar nsHtml5NamedCharacters::NAME_301[] = {
  'L', 's', 'c', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_301[] = {
  0x2112
};
PRUnichar nsHtml5NamedCharacters::NAME_302[] = {
  'L', 's', 'h', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_302[] = {
  0x21b0
};
PRUnichar nsHtml5NamedCharacters::NAME_303[] = {
  'L', 's', 't', 'r', 'o', 'k', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_303[] = {
  0x0141
};
PRUnichar nsHtml5NamedCharacters::NAME_304[] = {
  'L', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_304[] = {
  0x226a
};
PRUnichar nsHtml5NamedCharacters::NAME_305[] = {
  'M', 'a', 'p', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_305[] = {
  0x2905
};
PRUnichar nsHtml5NamedCharacters::NAME_306[] = {
  'M', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_306[] = {
  0x041c
};
PRUnichar nsHtml5NamedCharacters::NAME_307[] = {
  'M', 'e', 'd', 'i', 'u', 'm', 'S', 'p', 'a', 'c', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_307[] = {
  0x205f
};
PRUnichar nsHtml5NamedCharacters::NAME_308[] = {
  'M', 'e', 'l', 'l', 'i', 'n', 't', 'r', 'f', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_308[] = {
  0x2133
};
PRUnichar nsHtml5NamedCharacters::NAME_309[] = {
  'M', 'f', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_309[] = {
  0xd835, 0xdd10
};
PRUnichar nsHtml5NamedCharacters::NAME_310[] = {
  'M', 'i', 'n', 'u', 's', 'P', 'l', 'u', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_310[] = {
  0x2213
};
PRUnichar nsHtml5NamedCharacters::NAME_311[] = {
  'M', 'o', 'p', 'f', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_311[] = {
  0xd835, 0xdd44
};
PRUnichar nsHtml5NamedCharacters::NAME_312[] = {
  'M', 's', 'c', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_312[] = {
  0x2133
};
PRUnichar nsHtml5NamedCharacters::NAME_313[] = {
  'M', 'u', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_313[] = {
  0x039c
};
PRUnichar nsHtml5NamedCharacters::NAME_314[] = {
  'N', 'J', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_314[] = {
  0x040a
};
PRUnichar nsHtml5NamedCharacters::NAME_315[] = {
  'N', 'a', 'c', 'u', 't', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_315[] = {
  0x0143
};
PRUnichar nsHtml5NamedCharacters::NAME_316[] = {
  'N', 'c', 'a', 'r', 'o', 'n', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_316[] = {
  0x0147
};
PRUnichar nsHtml5NamedCharacters::NAME_317[] = {
  'N', 'c', 'e', 'd', 'i', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_317[] = {
  0x0145
};
PRUnichar nsHtml5NamedCharacters::NAME_318[] = {
  'N', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_318[] = {
  0x041d
};
PRUnichar nsHtml5NamedCharacters::NAME_319[] = {
  'N', 'e', 'g', 'a', 't', 'i', 'v', 'e', 'M', 'e', 'd', 'i', 'u', 'm', 'S', 'p', 'a', 'c', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_319[] = {
  0x200b
};
PRUnichar nsHtml5NamedCharacters::NAME_320[] = {
  'N', 'e', 'g', 'a', 't', 'i', 'v', 'e', 'T', 'h', 'i', 'c', 'k', 'S', 'p', 'a', 'c', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_320[] = {
  0x200b
};
PRUnichar nsHtml5NamedCharacters::NAME_321[] = {
  'N', 'e', 'g', 'a', 't', 'i', 'v', 'e', 'T', 'h', 'i', 'n', 'S', 'p', 'a', 'c', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_321[] = {
  0x200b
};
PRUnichar nsHtml5NamedCharacters::NAME_322[] = {
  'N', 'e', 'g', 'a', 't', 'i', 'v', 'e', 'V', 'e', 'r', 'y', 'T', 'h', 'i', 'n', 'S', 'p', 'a', 'c', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_322[] = {
  0x200b
};
PRUnichar nsHtml5NamedCharacters::NAME_323[] = {
  'N', 'e', 's', 't', 'e', 'd', 'G', 'r', 'e', 'a', 't', 'e', 'r', 'G', 'r', 'e', 'a', 't', 'e', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_323[] = {
  0x226b
};
PRUnichar nsHtml5NamedCharacters::NAME_324[] = {
  'N', 'e', 's', 't', 'e', 'd', 'L', 'e', 's', 's', 'L', 'e', 's', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_324[] = {
  0x226a
};
PRUnichar nsHtml5NamedCharacters::NAME_325[] = {
  'N', 'e', 'w', 'L', 'i', 'n', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_325[] = {
  0x000a
};
PRUnichar nsHtml5NamedCharacters::NAME_326[] = {
  'N', 'f', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_326[] = {
  0xd835, 0xdd11
};
PRUnichar nsHtml5NamedCharacters::NAME_327[] = {
  'N', 'o', 'B', 'r', 'e', 'a', 'k', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_327[] = {
  0x2060
};
PRUnichar nsHtml5NamedCharacters::NAME_328[] = {
  'N', 'o', 'n', 'B', 'r', 'e', 'a', 'k', 'i', 'n', 'g', 'S', 'p', 'a', 'c', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_328[] = {
  0x00a0
};
PRUnichar nsHtml5NamedCharacters::NAME_329[] = {
  'N', 'o', 'p', 'f', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_329[] = {
  0x2115
};
PRUnichar nsHtml5NamedCharacters::NAME_330[] = {
  'N', 'o', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_330[] = {
  0x2aec
};
PRUnichar nsHtml5NamedCharacters::NAME_331[] = {
  'N', 'o', 't', 'C', 'o', 'n', 'g', 'r', 'u', 'e', 'n', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_331[] = {
  0x2262
};
PRUnichar nsHtml5NamedCharacters::NAME_332[] = {
  'N', 'o', 't', 'C', 'u', 'p', 'C', 'a', 'p', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_332[] = {
  0x226d
};
PRUnichar nsHtml5NamedCharacters::NAME_333[] = {
  'N', 'o', 't', 'D', 'o', 'u', 'b', 'l', 'e', 'V', 'e', 'r', 't', 'i', 'c', 'a', 'l', 'B', 'a', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_333[] = {
  0x2226
};
PRUnichar nsHtml5NamedCharacters::NAME_334[] = {
  'N', 'o', 't', 'E', 'l', 'e', 'm', 'e', 'n', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_334[] = {
  0x2209
};
PRUnichar nsHtml5NamedCharacters::NAME_335[] = {
  'N', 'o', 't', 'E', 'q', 'u', 'a', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_335[] = {
  0x2260
};
PRUnichar nsHtml5NamedCharacters::NAME_336[] = {
  'N', 'o', 't', 'E', 'x', 'i', 's', 't', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_336[] = {
  0x2204
};
PRUnichar nsHtml5NamedCharacters::NAME_337[] = {
  'N', 'o', 't', 'G', 'r', 'e', 'a', 't', 'e', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_337[] = {
  0x226f
};
PRUnichar nsHtml5NamedCharacters::NAME_338[] = {
  'N', 'o', 't', 'G', 'r', 'e', 'a', 't', 'e', 'r', 'E', 'q', 'u', 'a', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_338[] = {
  0x2271
};
PRUnichar nsHtml5NamedCharacters::NAME_339[] = {
  'N', 'o', 't', 'G', 'r', 'e', 'a', 't', 'e', 'r', 'L', 'e', 's', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_339[] = {
  0x2279
};
PRUnichar nsHtml5NamedCharacters::NAME_340[] = {
  'N', 'o', 't', 'G', 'r', 'e', 'a', 't', 'e', 'r', 'T', 'i', 'l', 'd', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_340[] = {
  0x2275
};
PRUnichar nsHtml5NamedCharacters::NAME_341[] = {
  'N', 'o', 't', 'L', 'e', 'f', 't', 'T', 'r', 'i', 'a', 'n', 'g', 'l', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_341[] = {
  0x22ea
};
PRUnichar nsHtml5NamedCharacters::NAME_342[] = {
  'N', 'o', 't', 'L', 'e', 'f', 't', 'T', 'r', 'i', 'a', 'n', 'g', 'l', 'e', 'E', 'q', 'u', 'a', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_342[] = {
  0x22ec
};
PRUnichar nsHtml5NamedCharacters::NAME_343[] = {
  'N', 'o', 't', 'L', 'e', 's', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_343[] = {
  0x226e
};
PRUnichar nsHtml5NamedCharacters::NAME_344[] = {
  'N', 'o', 't', 'L', 'e', 's', 's', 'E', 'q', 'u', 'a', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_344[] = {
  0x2270
};
PRUnichar nsHtml5NamedCharacters::NAME_345[] = {
  'N', 'o', 't', 'L', 'e', 's', 's', 'G', 'r', 'e', 'a', 't', 'e', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_345[] = {
  0x2278
};
PRUnichar nsHtml5NamedCharacters::NAME_346[] = {
  'N', 'o', 't', 'L', 'e', 's', 's', 'T', 'i', 'l', 'd', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_346[] = {
  0x2274
};
PRUnichar nsHtml5NamedCharacters::NAME_347[] = {
  'N', 'o', 't', 'P', 'r', 'e', 'c', 'e', 'd', 'e', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_347[] = {
  0x2280
};
PRUnichar nsHtml5NamedCharacters::NAME_348[] = {
  'N', 'o', 't', 'P', 'r', 'e', 'c', 'e', 'd', 'e', 's', 'S', 'l', 'a', 'n', 't', 'E', 'q', 'u', 'a', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_348[] = {
  0x22e0
};
PRUnichar nsHtml5NamedCharacters::NAME_349[] = {
  'N', 'o', 't', 'R', 'e', 'v', 'e', 'r', 's', 'e', 'E', 'l', 'e', 'm', 'e', 'n', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_349[] = {
  0x220c
};
PRUnichar nsHtml5NamedCharacters::NAME_350[] = {
  'N', 'o', 't', 'R', 'i', 'g', 'h', 't', 'T', 'r', 'i', 'a', 'n', 'g', 'l', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_350[] = {
  0x22eb
};
PRUnichar nsHtml5NamedCharacters::NAME_351[] = {
  'N', 'o', 't', 'R', 'i', 'g', 'h', 't', 'T', 'r', 'i', 'a', 'n', 'g', 'l', 'e', 'E', 'q', 'u', 'a', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_351[] = {
  0x22ed
};
PRUnichar nsHtml5NamedCharacters::NAME_352[] = {
  'N', 'o', 't', 'S', 'q', 'u', 'a', 'r', 'e', 'S', 'u', 'b', 's', 'e', 't', 'E', 'q', 'u', 'a', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_352[] = {
  0x22e2
};
PRUnichar nsHtml5NamedCharacters::NAME_353[] = {
  'N', 'o', 't', 'S', 'q', 'u', 'a', 'r', 'e', 'S', 'u', 'p', 'e', 'r', 's', 'e', 't', 'E', 'q', 'u', 'a', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_353[] = {
  0x22e3
};
PRUnichar nsHtml5NamedCharacters::NAME_354[] = {
  'N', 'o', 't', 'S', 'u', 'b', 's', 'e', 't', 'E', 'q', 'u', 'a', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_354[] = {
  0x2288
};
PRUnichar nsHtml5NamedCharacters::NAME_355[] = {
  'N', 'o', 't', 'S', 'u', 'c', 'c', 'e', 'e', 'd', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_355[] = {
  0x2281
};
PRUnichar nsHtml5NamedCharacters::NAME_356[] = {
  'N', 'o', 't', 'S', 'u', 'c', 'c', 'e', 'e', 'd', 's', 'S', 'l', 'a', 'n', 't', 'E', 'q', 'u', 'a', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_356[] = {
  0x22e1
};
PRUnichar nsHtml5NamedCharacters::NAME_357[] = {
  'N', 'o', 't', 'S', 'u', 'p', 'e', 'r', 's', 'e', 't', 'E', 'q', 'u', 'a', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_357[] = {
  0x2289
};
PRUnichar nsHtml5NamedCharacters::NAME_358[] = {
  'N', 'o', 't', 'T', 'i', 'l', 'd', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_358[] = {
  0x2241
};
PRUnichar nsHtml5NamedCharacters::NAME_359[] = {
  'N', 'o', 't', 'T', 'i', 'l', 'd', 'e', 'E', 'q', 'u', 'a', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_359[] = {
  0x2244
};
PRUnichar nsHtml5NamedCharacters::NAME_360[] = {
  'N', 'o', 't', 'T', 'i', 'l', 'd', 'e', 'F', 'u', 'l', 'l', 'E', 'q', 'u', 'a', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_360[] = {
  0x2247
};
PRUnichar nsHtml5NamedCharacters::NAME_361[] = {
  'N', 'o', 't', 'T', 'i', 'l', 'd', 'e', 'T', 'i', 'l', 'd', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_361[] = {
  0x2249
};
PRUnichar nsHtml5NamedCharacters::NAME_362[] = {
  'N', 'o', 't', 'V', 'e', 'r', 't', 'i', 'c', 'a', 'l', 'B', 'a', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_362[] = {
  0x2224
};
PRUnichar nsHtml5NamedCharacters::NAME_363[] = {
  'N', 's', 'c', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_363[] = {
  0xd835, 0xdca9
};
PRUnichar nsHtml5NamedCharacters::NAME_364[] = {
  'N', 't', 'i', 'l', 'd', 'e'
};
PRUnichar nsHtml5NamedCharacters::VALUE_364[] = {
  0x00d1
};
PRUnichar nsHtml5NamedCharacters::NAME_365[] = {
  'N', 't', 'i', 'l', 'd', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_365[] = {
  0x00d1
};
PRUnichar nsHtml5NamedCharacters::NAME_366[] = {
  'N', 'u', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_366[] = {
  0x039d
};
PRUnichar nsHtml5NamedCharacters::NAME_367[] = {
  'O', 'E', 'l', 'i', 'g', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_367[] = {
  0x0152
};
PRUnichar nsHtml5NamedCharacters::NAME_368[] = {
  'O', 'a', 'c', 'u', 't', 'e'
};
PRUnichar nsHtml5NamedCharacters::VALUE_368[] = {
  0x00d3
};
PRUnichar nsHtml5NamedCharacters::NAME_369[] = {
  'O', 'a', 'c', 'u', 't', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_369[] = {
  0x00d3
};
PRUnichar nsHtml5NamedCharacters::NAME_370[] = {
  'O', 'c', 'i', 'r', 'c'
};
PRUnichar nsHtml5NamedCharacters::VALUE_370[] = {
  0x00d4
};
PRUnichar nsHtml5NamedCharacters::NAME_371[] = {
  'O', 'c', 'i', 'r', 'c', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_371[] = {
  0x00d4
};
PRUnichar nsHtml5NamedCharacters::NAME_372[] = {
  'O', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_372[] = {
  0x041e
};
PRUnichar nsHtml5NamedCharacters::NAME_373[] = {
  'O', 'd', 'b', 'l', 'a', 'c', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_373[] = {
  0x0150
};
PRUnichar nsHtml5NamedCharacters::NAME_374[] = {
  'O', 'f', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_374[] = {
  0xd835, 0xdd12
};
PRUnichar nsHtml5NamedCharacters::NAME_375[] = {
  'O', 'g', 'r', 'a', 'v', 'e'
};
PRUnichar nsHtml5NamedCharacters::VALUE_375[] = {
  0x00d2
};
PRUnichar nsHtml5NamedCharacters::NAME_376[] = {
  'O', 'g', 'r', 'a', 'v', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_376[] = {
  0x00d2
};
PRUnichar nsHtml5NamedCharacters::NAME_377[] = {
  'O', 'm', 'a', 'c', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_377[] = {
  0x014c
};
PRUnichar nsHtml5NamedCharacters::NAME_378[] = {
  'O', 'm', 'e', 'g', 'a', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_378[] = {
  0x03a9
};
PRUnichar nsHtml5NamedCharacters::NAME_379[] = {
  'O', 'm', 'i', 'c', 'r', 'o', 'n', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_379[] = {
  0x039f
};
PRUnichar nsHtml5NamedCharacters::NAME_380[] = {
  'O', 'o', 'p', 'f', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_380[] = {
  0xd835, 0xdd46
};
PRUnichar nsHtml5NamedCharacters::NAME_381[] = {
  'O', 'p', 'e', 'n', 'C', 'u', 'r', 'l', 'y', 'D', 'o', 'u', 'b', 'l', 'e', 'Q', 'u', 'o', 't', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_381[] = {
  0x201c
};
PRUnichar nsHtml5NamedCharacters::NAME_382[] = {
  'O', 'p', 'e', 'n', 'C', 'u', 'r', 'l', 'y', 'Q', 'u', 'o', 't', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_382[] = {
  0x2018
};
PRUnichar nsHtml5NamedCharacters::NAME_383[] = {
  'O', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_383[] = {
  0x2a54
};
PRUnichar nsHtml5NamedCharacters::NAME_384[] = {
  'O', 's', 'c', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_384[] = {
  0xd835, 0xdcaa
};
PRUnichar nsHtml5NamedCharacters::NAME_385[] = {
  'O', 's', 'l', 'a', 's', 'h'
};
PRUnichar nsHtml5NamedCharacters::VALUE_385[] = {
  0x00d8
};
PRUnichar nsHtml5NamedCharacters::NAME_386[] = {
  'O', 's', 'l', 'a', 's', 'h', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_386[] = {
  0x00d8
};
PRUnichar nsHtml5NamedCharacters::NAME_387[] = {
  'O', 't', 'i', 'l', 'd', 'e'
};
PRUnichar nsHtml5NamedCharacters::VALUE_387[] = {
  0x00d5
};
PRUnichar nsHtml5NamedCharacters::NAME_388[] = {
  'O', 't', 'i', 'l', 'd', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_388[] = {
  0x00d5
};
PRUnichar nsHtml5NamedCharacters::NAME_389[] = {
  'O', 't', 'i', 'm', 'e', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_389[] = {
  0x2a37
};
PRUnichar nsHtml5NamedCharacters::NAME_390[] = {
  'O', 'u', 'm', 'l'
};
PRUnichar nsHtml5NamedCharacters::VALUE_390[] = {
  0x00d6
};
PRUnichar nsHtml5NamedCharacters::NAME_391[] = {
  'O', 'u', 'm', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_391[] = {
  0x00d6
};
PRUnichar nsHtml5NamedCharacters::NAME_392[] = {
  'O', 'v', 'e', 'r', 'B', 'a', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_392[] = {
  0x00af
};
PRUnichar nsHtml5NamedCharacters::NAME_393[] = {
  'O', 'v', 'e', 'r', 'B', 'r', 'a', 'c', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_393[] = {
  0x23de
};
PRUnichar nsHtml5NamedCharacters::NAME_394[] = {
  'O', 'v', 'e', 'r', 'B', 'r', 'a', 'c', 'k', 'e', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_394[] = {
  0x23b4
};
PRUnichar nsHtml5NamedCharacters::NAME_395[] = {
  'O', 'v', 'e', 'r', 'P', 'a', 'r', 'e', 'n', 't', 'h', 'e', 's', 'i', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_395[] = {
  0x23dc
};
PRUnichar nsHtml5NamedCharacters::NAME_396[] = {
  'P', 'a', 'r', 't', 'i', 'a', 'l', 'D', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_396[] = {
  0x2202
};
PRUnichar nsHtml5NamedCharacters::NAME_397[] = {
  'P', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_397[] = {
  0x041f
};
PRUnichar nsHtml5NamedCharacters::NAME_398[] = {
  'P', 'f', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_398[] = {
  0xd835, 0xdd13
};
PRUnichar nsHtml5NamedCharacters::NAME_399[] = {
  'P', 'h', 'i', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_399[] = {
  0x03a6
};
PRUnichar nsHtml5NamedCharacters::NAME_400[] = {
  'P', 'i', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_400[] = {
  0x03a0
};
PRUnichar nsHtml5NamedCharacters::NAME_401[] = {
  'P', 'l', 'u', 's', 'M', 'i', 'n', 'u', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_401[] = {
  0x00b1
};
PRUnichar nsHtml5NamedCharacters::NAME_402[] = {
  'P', 'o', 'i', 'n', 'c', 'a', 'r', 'e', 'p', 'l', 'a', 'n', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_402[] = {
  0x210c
};
PRUnichar nsHtml5NamedCharacters::NAME_403[] = {
  'P', 'o', 'p', 'f', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_403[] = {
  0x2119
};
PRUnichar nsHtml5NamedCharacters::NAME_404[] = {
  'P', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_404[] = {
  0x2abb
};
PRUnichar nsHtml5NamedCharacters::NAME_405[] = {
  'P', 'r', 'e', 'c', 'e', 'd', 'e', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_405[] = {
  0x227a
};
PRUnichar nsHtml5NamedCharacters::NAME_406[] = {
  'P', 'r', 'e', 'c', 'e', 'd', 'e', 's', 'E', 'q', 'u', 'a', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_406[] = {
  0x2aaf
};
PRUnichar nsHtml5NamedCharacters::NAME_407[] = {
  'P', 'r', 'e', 'c', 'e', 'd', 'e', 's', 'S', 'l', 'a', 'n', 't', 'E', 'q', 'u', 'a', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_407[] = {
  0x227c
};
PRUnichar nsHtml5NamedCharacters::NAME_408[] = {
  'P', 'r', 'e', 'c', 'e', 'd', 'e', 's', 'T', 'i', 'l', 'd', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_408[] = {
  0x227e
};
PRUnichar nsHtml5NamedCharacters::NAME_409[] = {
  'P', 'r', 'i', 'm', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_409[] = {
  0x2033
};
PRUnichar nsHtml5NamedCharacters::NAME_410[] = {
  'P', 'r', 'o', 'd', 'u', 'c', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_410[] = {
  0x220f
};
PRUnichar nsHtml5NamedCharacters::NAME_411[] = {
  'P', 'r', 'o', 'p', 'o', 'r', 't', 'i', 'o', 'n', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_411[] = {
  0x2237
};
PRUnichar nsHtml5NamedCharacters::NAME_412[] = {
  'P', 'r', 'o', 'p', 'o', 'r', 't', 'i', 'o', 'n', 'a', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_412[] = {
  0x221d
};
PRUnichar nsHtml5NamedCharacters::NAME_413[] = {
  'P', 's', 'c', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_413[] = {
  0xd835, 0xdcab
};
PRUnichar nsHtml5NamedCharacters::NAME_414[] = {
  'P', 's', 'i', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_414[] = {
  0x03a8
};
PRUnichar nsHtml5NamedCharacters::NAME_415[] = {
  'Q', 'U', 'O', 'T'
};
PRUnichar nsHtml5NamedCharacters::VALUE_415[] = {
  0x0022
};
PRUnichar nsHtml5NamedCharacters::NAME_416[] = {
  'Q', 'U', 'O', 'T', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_416[] = {
  0x0022
};
PRUnichar nsHtml5NamedCharacters::NAME_417[] = {
  'Q', 'f', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_417[] = {
  0xd835, 0xdd14
};
PRUnichar nsHtml5NamedCharacters::NAME_418[] = {
  'Q', 'o', 'p', 'f', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_418[] = {
  0x211a
};
PRUnichar nsHtml5NamedCharacters::NAME_419[] = {
  'Q', 's', 'c', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_419[] = {
  0xd835, 0xdcac
};
PRUnichar nsHtml5NamedCharacters::NAME_420[] = {
  'R', 'B', 'a', 'r', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_420[] = {
  0x2910
};
PRUnichar nsHtml5NamedCharacters::NAME_421[] = {
  'R', 'E', 'G'
};
PRUnichar nsHtml5NamedCharacters::VALUE_421[] = {
  0x00ae
};
PRUnichar nsHtml5NamedCharacters::NAME_422[] = {
  'R', 'E', 'G', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_422[] = {
  0x00ae
};
PRUnichar nsHtml5NamedCharacters::NAME_423[] = {
  'R', 'a', 'c', 'u', 't', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_423[] = {
  0x0154
};
PRUnichar nsHtml5NamedCharacters::NAME_424[] = {
  'R', 'a', 'n', 'g', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_424[] = {
  0x27eb
};
PRUnichar nsHtml5NamedCharacters::NAME_425[] = {
  'R', 'a', 'r', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_425[] = {
  0x21a0
};
PRUnichar nsHtml5NamedCharacters::NAME_426[] = {
  'R', 'a', 'r', 'r', 't', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_426[] = {
  0x2916
};
PRUnichar nsHtml5NamedCharacters::NAME_427[] = {
  'R', 'c', 'a', 'r', 'o', 'n', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_427[] = {
  0x0158
};
PRUnichar nsHtml5NamedCharacters::NAME_428[] = {
  'R', 'c', 'e', 'd', 'i', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_428[] = {
  0x0156
};
PRUnichar nsHtml5NamedCharacters::NAME_429[] = {
  'R', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_429[] = {
  0x0420
};
PRUnichar nsHtml5NamedCharacters::NAME_430[] = {
  'R', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_430[] = {
  0x211c
};
PRUnichar nsHtml5NamedCharacters::NAME_431[] = {
  'R', 'e', 'v', 'e', 'r', 's', 'e', 'E', 'l', 'e', 'm', 'e', 'n', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_431[] = {
  0x220b
};
PRUnichar nsHtml5NamedCharacters::NAME_432[] = {
  'R', 'e', 'v', 'e', 'r', 's', 'e', 'E', 'q', 'u', 'i', 'l', 'i', 'b', 'r', 'i', 'u', 'm', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_432[] = {
  0x21cb
};
PRUnichar nsHtml5NamedCharacters::NAME_433[] = {
  'R', 'e', 'v', 'e', 'r', 's', 'e', 'U', 'p', 'E', 'q', 'u', 'i', 'l', 'i', 'b', 'r', 'i', 'u', 'm', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_433[] = {
  0x296f
};
PRUnichar nsHtml5NamedCharacters::NAME_434[] = {
  'R', 'f', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_434[] = {
  0x211c
};
PRUnichar nsHtml5NamedCharacters::NAME_435[] = {
  'R', 'h', 'o', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_435[] = {
  0x03a1
};
PRUnichar nsHtml5NamedCharacters::NAME_436[] = {
  'R', 'i', 'g', 'h', 't', 'A', 'n', 'g', 'l', 'e', 'B', 'r', 'a', 'c', 'k', 'e', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_436[] = {
  0x27e9
};
PRUnichar nsHtml5NamedCharacters::NAME_437[] = {
  'R', 'i', 'g', 'h', 't', 'A', 'r', 'r', 'o', 'w', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_437[] = {
  0x2192
};
PRUnichar nsHtml5NamedCharacters::NAME_438[] = {
  'R', 'i', 'g', 'h', 't', 'A', 'r', 'r', 'o', 'w', 'B', 'a', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_438[] = {
  0x21e5
};
PRUnichar nsHtml5NamedCharacters::NAME_439[] = {
  'R', 'i', 'g', 'h', 't', 'A', 'r', 'r', 'o', 'w', 'L', 'e', 'f', 't', 'A', 'r', 'r', 'o', 'w', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_439[] = {
  0x21c4
};
PRUnichar nsHtml5NamedCharacters::NAME_440[] = {
  'R', 'i', 'g', 'h', 't', 'C', 'e', 'i', 'l', 'i', 'n', 'g', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_440[] = {
  0x2309
};
PRUnichar nsHtml5NamedCharacters::NAME_441[] = {
  'R', 'i', 'g', 'h', 't', 'D', 'o', 'u', 'b', 'l', 'e', 'B', 'r', 'a', 'c', 'k', 'e', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_441[] = {
  0x27e7
};
PRUnichar nsHtml5NamedCharacters::NAME_442[] = {
  'R', 'i', 'g', 'h', 't', 'D', 'o', 'w', 'n', 'T', 'e', 'e', 'V', 'e', 'c', 't', 'o', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_442[] = {
  0x295d
};
PRUnichar nsHtml5NamedCharacters::NAME_443[] = {
  'R', 'i', 'g', 'h', 't', 'D', 'o', 'w', 'n', 'V', 'e', 'c', 't', 'o', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_443[] = {
  0x21c2
};
PRUnichar nsHtml5NamedCharacters::NAME_444[] = {
  'R', 'i', 'g', 'h', 't', 'D', 'o', 'w', 'n', 'V', 'e', 'c', 't', 'o', 'r', 'B', 'a', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_444[] = {
  0x2955
};
PRUnichar nsHtml5NamedCharacters::NAME_445[] = {
  'R', 'i', 'g', 'h', 't', 'F', 'l', 'o', 'o', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_445[] = {
  0x230b
};
PRUnichar nsHtml5NamedCharacters::NAME_446[] = {
  'R', 'i', 'g', 'h', 't', 'T', 'e', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_446[] = {
  0x22a2
};
PRUnichar nsHtml5NamedCharacters::NAME_447[] = {
  'R', 'i', 'g', 'h', 't', 'T', 'e', 'e', 'A', 'r', 'r', 'o', 'w', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_447[] = {
  0x21a6
};
PRUnichar nsHtml5NamedCharacters::NAME_448[] = {
  'R', 'i', 'g', 'h', 't', 'T', 'e', 'e', 'V', 'e', 'c', 't', 'o', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_448[] = {
  0x295b
};
PRUnichar nsHtml5NamedCharacters::NAME_449[] = {
  'R', 'i', 'g', 'h', 't', 'T', 'r', 'i', 'a', 'n', 'g', 'l', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_449[] = {
  0x22b3
};
PRUnichar nsHtml5NamedCharacters::NAME_450[] = {
  'R', 'i', 'g', 'h', 't', 'T', 'r', 'i', 'a', 'n', 'g', 'l', 'e', 'B', 'a', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_450[] = {
  0x29d0
};
PRUnichar nsHtml5NamedCharacters::NAME_451[] = {
  'R', 'i', 'g', 'h', 't', 'T', 'r', 'i', 'a', 'n', 'g', 'l', 'e', 'E', 'q', 'u', 'a', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_451[] = {
  0x22b5
};
PRUnichar nsHtml5NamedCharacters::NAME_452[] = {
  'R', 'i', 'g', 'h', 't', 'U', 'p', 'D', 'o', 'w', 'n', 'V', 'e', 'c', 't', 'o', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_452[] = {
  0x294f
};
PRUnichar nsHtml5NamedCharacters::NAME_453[] = {
  'R', 'i', 'g', 'h', 't', 'U', 'p', 'T', 'e', 'e', 'V', 'e', 'c', 't', 'o', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_453[] = {
  0x295c
};
PRUnichar nsHtml5NamedCharacters::NAME_454[] = {
  'R', 'i', 'g', 'h', 't', 'U', 'p', 'V', 'e', 'c', 't', 'o', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_454[] = {
  0x21be
};
PRUnichar nsHtml5NamedCharacters::NAME_455[] = {
  'R', 'i', 'g', 'h', 't', 'U', 'p', 'V', 'e', 'c', 't', 'o', 'r', 'B', 'a', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_455[] = {
  0x2954
};
PRUnichar nsHtml5NamedCharacters::NAME_456[] = {
  'R', 'i', 'g', 'h', 't', 'V', 'e', 'c', 't', 'o', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_456[] = {
  0x21c0
};
PRUnichar nsHtml5NamedCharacters::NAME_457[] = {
  'R', 'i', 'g', 'h', 't', 'V', 'e', 'c', 't', 'o', 'r', 'B', 'a', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_457[] = {
  0x2953
};
PRUnichar nsHtml5NamedCharacters::NAME_458[] = {
  'R', 'i', 'g', 'h', 't', 'a', 'r', 'r', 'o', 'w', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_458[] = {
  0x21d2
};
PRUnichar nsHtml5NamedCharacters::NAME_459[] = {
  'R', 'o', 'p', 'f', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_459[] = {
  0x211d
};
PRUnichar nsHtml5NamedCharacters::NAME_460[] = {
  'R', 'o', 'u', 'n', 'd', 'I', 'm', 'p', 'l', 'i', 'e', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_460[] = {
  0x2970
};
PRUnichar nsHtml5NamedCharacters::NAME_461[] = {
  'R', 'r', 'i', 'g', 'h', 't', 'a', 'r', 'r', 'o', 'w', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_461[] = {
  0x21db
};
PRUnichar nsHtml5NamedCharacters::NAME_462[] = {
  'R', 's', 'c', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_462[] = {
  0x211b
};
PRUnichar nsHtml5NamedCharacters::NAME_463[] = {
  'R', 's', 'h', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_463[] = {
  0x21b1
};
PRUnichar nsHtml5NamedCharacters::NAME_464[] = {
  'R', 'u', 'l', 'e', 'D', 'e', 'l', 'a', 'y', 'e', 'd', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_464[] = {
  0x29f4
};
PRUnichar nsHtml5NamedCharacters::NAME_465[] = {
  'S', 'H', 'C', 'H', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_465[] = {
  0x0429
};
PRUnichar nsHtml5NamedCharacters::NAME_466[] = {
  'S', 'H', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_466[] = {
  0x0428
};
PRUnichar nsHtml5NamedCharacters::NAME_467[] = {
  'S', 'O', 'F', 'T', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_467[] = {
  0x042c
};
PRUnichar nsHtml5NamedCharacters::NAME_468[] = {
  'S', 'a', 'c', 'u', 't', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_468[] = {
  0x015a
};
PRUnichar nsHtml5NamedCharacters::NAME_469[] = {
  'S', 'c', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_469[] = {
  0x2abc
};
PRUnichar nsHtml5NamedCharacters::NAME_470[] = {
  'S', 'c', 'a', 'r', 'o', 'n', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_470[] = {
  0x0160
};
PRUnichar nsHtml5NamedCharacters::NAME_471[] = {
  'S', 'c', 'e', 'd', 'i', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_471[] = {
  0x015e
};
PRUnichar nsHtml5NamedCharacters::NAME_472[] = {
  'S', 'c', 'i', 'r', 'c', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_472[] = {
  0x015c
};
PRUnichar nsHtml5NamedCharacters::NAME_473[] = {
  'S', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_473[] = {
  0x0421
};
PRUnichar nsHtml5NamedCharacters::NAME_474[] = {
  'S', 'f', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_474[] = {
  0xd835, 0xdd16
};
PRUnichar nsHtml5NamedCharacters::NAME_475[] = {
  'S', 'h', 'o', 'r', 't', 'D', 'o', 'w', 'n', 'A', 'r', 'r', 'o', 'w', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_475[] = {
  0x2193
};
PRUnichar nsHtml5NamedCharacters::NAME_476[] = {
  'S', 'h', 'o', 'r', 't', 'L', 'e', 'f', 't', 'A', 'r', 'r', 'o', 'w', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_476[] = {
  0x2190
};
PRUnichar nsHtml5NamedCharacters::NAME_477[] = {
  'S', 'h', 'o', 'r', 't', 'R', 'i', 'g', 'h', 't', 'A', 'r', 'r', 'o', 'w', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_477[] = {
  0x2192
};
PRUnichar nsHtml5NamedCharacters::NAME_478[] = {
  'S', 'h', 'o', 'r', 't', 'U', 'p', 'A', 'r', 'r', 'o', 'w', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_478[] = {
  0x2191
};
PRUnichar nsHtml5NamedCharacters::NAME_479[] = {
  'S', 'i', 'g', 'm', 'a', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_479[] = {
  0x03a3
};
PRUnichar nsHtml5NamedCharacters::NAME_480[] = {
  'S', 'm', 'a', 'l', 'l', 'C', 'i', 'r', 'c', 'l', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_480[] = {
  0x2218
};
PRUnichar nsHtml5NamedCharacters::NAME_481[] = {
  'S', 'o', 'p', 'f', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_481[] = {
  0xd835, 0xdd4a
};
PRUnichar nsHtml5NamedCharacters::NAME_482[] = {
  'S', 'q', 'r', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_482[] = {
  0x221a
};
PRUnichar nsHtml5NamedCharacters::NAME_483[] = {
  'S', 'q', 'u', 'a', 'r', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_483[] = {
  0x25a1
};
PRUnichar nsHtml5NamedCharacters::NAME_484[] = {
  'S', 'q', 'u', 'a', 'r', 'e', 'I', 'n', 't', 'e', 'r', 's', 'e', 'c', 't', 'i', 'o', 'n', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_484[] = {
  0x2293
};
PRUnichar nsHtml5NamedCharacters::NAME_485[] = {
  'S', 'q', 'u', 'a', 'r', 'e', 'S', 'u', 'b', 's', 'e', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_485[] = {
  0x228f
};
PRUnichar nsHtml5NamedCharacters::NAME_486[] = {
  'S', 'q', 'u', 'a', 'r', 'e', 'S', 'u', 'b', 's', 'e', 't', 'E', 'q', 'u', 'a', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_486[] = {
  0x2291
};
PRUnichar nsHtml5NamedCharacters::NAME_487[] = {
  'S', 'q', 'u', 'a', 'r', 'e', 'S', 'u', 'p', 'e', 'r', 's', 'e', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_487[] = {
  0x2290
};
PRUnichar nsHtml5NamedCharacters::NAME_488[] = {
  'S', 'q', 'u', 'a', 'r', 'e', 'S', 'u', 'p', 'e', 'r', 's', 'e', 't', 'E', 'q', 'u', 'a', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_488[] = {
  0x2292
};
PRUnichar nsHtml5NamedCharacters::NAME_489[] = {
  'S', 'q', 'u', 'a', 'r', 'e', 'U', 'n', 'i', 'o', 'n', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_489[] = {
  0x2294
};
PRUnichar nsHtml5NamedCharacters::NAME_490[] = {
  'S', 's', 'c', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_490[] = {
  0xd835, 0xdcae
};
PRUnichar nsHtml5NamedCharacters::NAME_491[] = {
  'S', 't', 'a', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_491[] = {
  0x22c6
};
PRUnichar nsHtml5NamedCharacters::NAME_492[] = {
  'S', 'u', 'b', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_492[] = {
  0x22d0
};
PRUnichar nsHtml5NamedCharacters::NAME_493[] = {
  'S', 'u', 'b', 's', 'e', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_493[] = {
  0x22d0
};
PRUnichar nsHtml5NamedCharacters::NAME_494[] = {
  'S', 'u', 'b', 's', 'e', 't', 'E', 'q', 'u', 'a', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_494[] = {
  0x2286
};
PRUnichar nsHtml5NamedCharacters::NAME_495[] = {
  'S', 'u', 'c', 'c', 'e', 'e', 'd', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_495[] = {
  0x227b
};
PRUnichar nsHtml5NamedCharacters::NAME_496[] = {
  'S', 'u', 'c', 'c', 'e', 'e', 'd', 's', 'E', 'q', 'u', 'a', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_496[] = {
  0x2ab0
};
PRUnichar nsHtml5NamedCharacters::NAME_497[] = {
  'S', 'u', 'c', 'c', 'e', 'e', 'd', 's', 'S', 'l', 'a', 'n', 't', 'E', 'q', 'u', 'a', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_497[] = {
  0x227d
};
PRUnichar nsHtml5NamedCharacters::NAME_498[] = {
  'S', 'u', 'c', 'c', 'e', 'e', 'd', 's', 'T', 'i', 'l', 'd', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_498[] = {
  0x227f
};
PRUnichar nsHtml5NamedCharacters::NAME_499[] = {
  'S', 'u', 'c', 'h', 'T', 'h', 'a', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_499[] = {
  0x220b
};
PRUnichar nsHtml5NamedCharacters::NAME_500[] = {
  'S', 'u', 'm', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_500[] = {
  0x2211
};
PRUnichar nsHtml5NamedCharacters::NAME_501[] = {
  'S', 'u', 'p', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_501[] = {
  0x22d1
};
PRUnichar nsHtml5NamedCharacters::NAME_502[] = {
  'S', 'u', 'p', 'e', 'r', 's', 'e', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_502[] = {
  0x2283
};
PRUnichar nsHtml5NamedCharacters::NAME_503[] = {
  'S', 'u', 'p', 'e', 'r', 's', 'e', 't', 'E', 'q', 'u', 'a', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_503[] = {
  0x2287
};
PRUnichar nsHtml5NamedCharacters::NAME_504[] = {
  'S', 'u', 'p', 's', 'e', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_504[] = {
  0x22d1
};
PRUnichar nsHtml5NamedCharacters::NAME_505[] = {
  'T', 'H', 'O', 'R', 'N'
};
PRUnichar nsHtml5NamedCharacters::VALUE_505[] = {
  0x00de
};
PRUnichar nsHtml5NamedCharacters::NAME_506[] = {
  'T', 'H', 'O', 'R', 'N', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_506[] = {
  0x00de
};
PRUnichar nsHtml5NamedCharacters::NAME_507[] = {
  'T', 'R', 'A', 'D', 'E', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_507[] = {
  0x2122
};
PRUnichar nsHtml5NamedCharacters::NAME_508[] = {
  'T', 'S', 'H', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_508[] = {
  0x040b
};
PRUnichar nsHtml5NamedCharacters::NAME_509[] = {
  'T', 'S', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_509[] = {
  0x0426
};
PRUnichar nsHtml5NamedCharacters::NAME_510[] = {
  'T', 'a', 'b', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_510[] = {
  0x0009
};
PRUnichar nsHtml5NamedCharacters::NAME_511[] = {
  'T', 'a', 'u', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_511[] = {
  0x03a4
};
PRUnichar nsHtml5NamedCharacters::NAME_512[] = {
  'T', 'c', 'a', 'r', 'o', 'n', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_512[] = {
  0x0164
};
PRUnichar nsHtml5NamedCharacters::NAME_513[] = {
  'T', 'c', 'e', 'd', 'i', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_513[] = {
  0x0162
};
PRUnichar nsHtml5NamedCharacters::NAME_514[] = {
  'T', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_514[] = {
  0x0422
};
PRUnichar nsHtml5NamedCharacters::NAME_515[] = {
  'T', 'f', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_515[] = {
  0xd835, 0xdd17
};
PRUnichar nsHtml5NamedCharacters::NAME_516[] = {
  'T', 'h', 'e', 'r', 'e', 'f', 'o', 'r', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_516[] = {
  0x2234
};
PRUnichar nsHtml5NamedCharacters::NAME_517[] = {
  'T', 'h', 'e', 't', 'a', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_517[] = {
  0x0398
};
PRUnichar nsHtml5NamedCharacters::NAME_518[] = {
  'T', 'h', 'i', 'n', 'S', 'p', 'a', 'c', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_518[] = {
  0x2009
};
PRUnichar nsHtml5NamedCharacters::NAME_519[] = {
  'T', 'i', 'l', 'd', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_519[] = {
  0x223c
};
PRUnichar nsHtml5NamedCharacters::NAME_520[] = {
  'T', 'i', 'l', 'd', 'e', 'E', 'q', 'u', 'a', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_520[] = {
  0x2243
};
PRUnichar nsHtml5NamedCharacters::NAME_521[] = {
  'T', 'i', 'l', 'd', 'e', 'F', 'u', 'l', 'l', 'E', 'q', 'u', 'a', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_521[] = {
  0x2245
};
PRUnichar nsHtml5NamedCharacters::NAME_522[] = {
  'T', 'i', 'l', 'd', 'e', 'T', 'i', 'l', 'd', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_522[] = {
  0x2248
};
PRUnichar nsHtml5NamedCharacters::NAME_523[] = {
  'T', 'o', 'p', 'f', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_523[] = {
  0xd835, 0xdd4b
};
PRUnichar nsHtml5NamedCharacters::NAME_524[] = {
  'T', 'r', 'i', 'p', 'l', 'e', 'D', 'o', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_524[] = {
  0x20db
};
PRUnichar nsHtml5NamedCharacters::NAME_525[] = {
  'T', 's', 'c', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_525[] = {
  0xd835, 0xdcaf
};
PRUnichar nsHtml5NamedCharacters::NAME_526[] = {
  'T', 's', 't', 'r', 'o', 'k', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_526[] = {
  0x0166
};
PRUnichar nsHtml5NamedCharacters::NAME_527[] = {
  'U', 'a', 'c', 'u', 't', 'e'
};
PRUnichar nsHtml5NamedCharacters::VALUE_527[] = {
  0x00da
};
PRUnichar nsHtml5NamedCharacters::NAME_528[] = {
  'U', 'a', 'c', 'u', 't', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_528[] = {
  0x00da
};
PRUnichar nsHtml5NamedCharacters::NAME_529[] = {
  'U', 'a', 'r', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_529[] = {
  0x219f
};
PRUnichar nsHtml5NamedCharacters::NAME_530[] = {
  'U', 'a', 'r', 'r', 'o', 'c', 'i', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_530[] = {
  0x2949
};
PRUnichar nsHtml5NamedCharacters::NAME_531[] = {
  'U', 'b', 'r', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_531[] = {
  0x040e
};
PRUnichar nsHtml5NamedCharacters::NAME_532[] = {
  'U', 'b', 'r', 'e', 'v', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_532[] = {
  0x016c
};
PRUnichar nsHtml5NamedCharacters::NAME_533[] = {
  'U', 'c', 'i', 'r', 'c'
};
PRUnichar nsHtml5NamedCharacters::VALUE_533[] = {
  0x00db
};
PRUnichar nsHtml5NamedCharacters::NAME_534[] = {
  'U', 'c', 'i', 'r', 'c', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_534[] = {
  0x00db
};
PRUnichar nsHtml5NamedCharacters::NAME_535[] = {
  'U', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_535[] = {
  0x0423
};
PRUnichar nsHtml5NamedCharacters::NAME_536[] = {
  'U', 'd', 'b', 'l', 'a', 'c', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_536[] = {
  0x0170
};
PRUnichar nsHtml5NamedCharacters::NAME_537[] = {
  'U', 'f', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_537[] = {
  0xd835, 0xdd18
};
PRUnichar nsHtml5NamedCharacters::NAME_538[] = {
  'U', 'g', 'r', 'a', 'v', 'e'
};
PRUnichar nsHtml5NamedCharacters::VALUE_538[] = {
  0x00d9
};
PRUnichar nsHtml5NamedCharacters::NAME_539[] = {
  'U', 'g', 'r', 'a', 'v', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_539[] = {
  0x00d9
};
PRUnichar nsHtml5NamedCharacters::NAME_540[] = {
  'U', 'm', 'a', 'c', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_540[] = {
  0x016a
};
PRUnichar nsHtml5NamedCharacters::NAME_541[] = {
  'U', 'n', 'd', 'e', 'r', 'B', 'a', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_541[] = {
  0x0332
};
PRUnichar nsHtml5NamedCharacters::NAME_542[] = {
  'U', 'n', 'd', 'e', 'r', 'B', 'r', 'a', 'c', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_542[] = {
  0x23df
};
PRUnichar nsHtml5NamedCharacters::NAME_543[] = {
  'U', 'n', 'd', 'e', 'r', 'B', 'r', 'a', 'c', 'k', 'e', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_543[] = {
  0x23b5
};
PRUnichar nsHtml5NamedCharacters::NAME_544[] = {
  'U', 'n', 'd', 'e', 'r', 'P', 'a', 'r', 'e', 'n', 't', 'h', 'e', 's', 'i', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_544[] = {
  0x23dd
};
PRUnichar nsHtml5NamedCharacters::NAME_545[] = {
  'U', 'n', 'i', 'o', 'n', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_545[] = {
  0x22c3
};
PRUnichar nsHtml5NamedCharacters::NAME_546[] = {
  'U', 'n', 'i', 'o', 'n', 'P', 'l', 'u', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_546[] = {
  0x228e
};
PRUnichar nsHtml5NamedCharacters::NAME_547[] = {
  'U', 'o', 'g', 'o', 'n', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_547[] = {
  0x0172
};
PRUnichar nsHtml5NamedCharacters::NAME_548[] = {
  'U', 'o', 'p', 'f', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_548[] = {
  0xd835, 0xdd4c
};
PRUnichar nsHtml5NamedCharacters::NAME_549[] = {
  'U', 'p', 'A', 'r', 'r', 'o', 'w', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_549[] = {
  0x2191
};
PRUnichar nsHtml5NamedCharacters::NAME_550[] = {
  'U', 'p', 'A', 'r', 'r', 'o', 'w', 'B', 'a', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_550[] = {
  0x2912
};
PRUnichar nsHtml5NamedCharacters::NAME_551[] = {
  'U', 'p', 'A', 'r', 'r', 'o', 'w', 'D', 'o', 'w', 'n', 'A', 'r', 'r', 'o', 'w', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_551[] = {
  0x21c5
};
PRUnichar nsHtml5NamedCharacters::NAME_552[] = {
  'U', 'p', 'D', 'o', 'w', 'n', 'A', 'r', 'r', 'o', 'w', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_552[] = {
  0x2195
};
PRUnichar nsHtml5NamedCharacters::NAME_553[] = {
  'U', 'p', 'E', 'q', 'u', 'i', 'l', 'i', 'b', 'r', 'i', 'u', 'm', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_553[] = {
  0x296e
};
PRUnichar nsHtml5NamedCharacters::NAME_554[] = {
  'U', 'p', 'T', 'e', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_554[] = {
  0x22a5
};
PRUnichar nsHtml5NamedCharacters::NAME_555[] = {
  'U', 'p', 'T', 'e', 'e', 'A', 'r', 'r', 'o', 'w', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_555[] = {
  0x21a5
};
PRUnichar nsHtml5NamedCharacters::NAME_556[] = {
  'U', 'p', 'a', 'r', 'r', 'o', 'w', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_556[] = {
  0x21d1
};
PRUnichar nsHtml5NamedCharacters::NAME_557[] = {
  'U', 'p', 'd', 'o', 'w', 'n', 'a', 'r', 'r', 'o', 'w', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_557[] = {
  0x21d5
};
PRUnichar nsHtml5NamedCharacters::NAME_558[] = {
  'U', 'p', 'p', 'e', 'r', 'L', 'e', 'f', 't', 'A', 'r', 'r', 'o', 'w', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_558[] = {
  0x2196
};
PRUnichar nsHtml5NamedCharacters::NAME_559[] = {
  'U', 'p', 'p', 'e', 'r', 'R', 'i', 'g', 'h', 't', 'A', 'r', 'r', 'o', 'w', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_559[] = {
  0x2197
};
PRUnichar nsHtml5NamedCharacters::NAME_560[] = {
  'U', 'p', 's', 'i', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_560[] = {
  0x03d2
};
PRUnichar nsHtml5NamedCharacters::NAME_561[] = {
  'U', 'p', 's', 'i', 'l', 'o', 'n', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_561[] = {
  0x03a5
};
PRUnichar nsHtml5NamedCharacters::NAME_562[] = {
  'U', 'r', 'i', 'n', 'g', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_562[] = {
  0x016e
};
PRUnichar nsHtml5NamedCharacters::NAME_563[] = {
  'U', 's', 'c', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_563[] = {
  0xd835, 0xdcb0
};
PRUnichar nsHtml5NamedCharacters::NAME_564[] = {
  'U', 't', 'i', 'l', 'd', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_564[] = {
  0x0168
};
PRUnichar nsHtml5NamedCharacters::NAME_565[] = {
  'U', 'u', 'm', 'l'
};
PRUnichar nsHtml5NamedCharacters::VALUE_565[] = {
  0x00dc
};
PRUnichar nsHtml5NamedCharacters::NAME_566[] = {
  'U', 'u', 'm', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_566[] = {
  0x00dc
};
PRUnichar nsHtml5NamedCharacters::NAME_567[] = {
  'V', 'D', 'a', 's', 'h', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_567[] = {
  0x22ab
};
PRUnichar nsHtml5NamedCharacters::NAME_568[] = {
  'V', 'b', 'a', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_568[] = {
  0x2aeb
};
PRUnichar nsHtml5NamedCharacters::NAME_569[] = {
  'V', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_569[] = {
  0x0412
};
PRUnichar nsHtml5NamedCharacters::NAME_570[] = {
  'V', 'd', 'a', 's', 'h', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_570[] = {
  0x22a9
};
PRUnichar nsHtml5NamedCharacters::NAME_571[] = {
  'V', 'd', 'a', 's', 'h', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_571[] = {
  0x2ae6
};
PRUnichar nsHtml5NamedCharacters::NAME_572[] = {
  'V', 'e', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_572[] = {
  0x22c1
};
PRUnichar nsHtml5NamedCharacters::NAME_573[] = {
  'V', 'e', 'r', 'b', 'a', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_573[] = {
  0x2016
};
PRUnichar nsHtml5NamedCharacters::NAME_574[] = {
  'V', 'e', 'r', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_574[] = {
  0x2016
};
PRUnichar nsHtml5NamedCharacters::NAME_575[] = {
  'V', 'e', 'r', 't', 'i', 'c', 'a', 'l', 'B', 'a', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_575[] = {
  0x2223
};
PRUnichar nsHtml5NamedCharacters::NAME_576[] = {
  'V', 'e', 'r', 't', 'i', 'c', 'a', 'l', 'L', 'i', 'n', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_576[] = {
  0x007c
};
PRUnichar nsHtml5NamedCharacters::NAME_577[] = {
  'V', 'e', 'r', 't', 'i', 'c', 'a', 'l', 'S', 'e', 'p', 'a', 'r', 'a', 't', 'o', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_577[] = {
  0x2758
};
PRUnichar nsHtml5NamedCharacters::NAME_578[] = {
  'V', 'e', 'r', 't', 'i', 'c', 'a', 'l', 'T', 'i', 'l', 'd', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_578[] = {
  0x2240
};
PRUnichar nsHtml5NamedCharacters::NAME_579[] = {
  'V', 'e', 'r', 'y', 'T', 'h', 'i', 'n', 'S', 'p', 'a', 'c', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_579[] = {
  0x200a
};
PRUnichar nsHtml5NamedCharacters::NAME_580[] = {
  'V', 'f', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_580[] = {
  0xd835, 0xdd19
};
PRUnichar nsHtml5NamedCharacters::NAME_581[] = {
  'V', 'o', 'p', 'f', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_581[] = {
  0xd835, 0xdd4d
};
PRUnichar nsHtml5NamedCharacters::NAME_582[] = {
  'V', 's', 'c', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_582[] = {
  0xd835, 0xdcb1
};
PRUnichar nsHtml5NamedCharacters::NAME_583[] = {
  'V', 'v', 'd', 'a', 's', 'h', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_583[] = {
  0x22aa
};
PRUnichar nsHtml5NamedCharacters::NAME_584[] = {
  'W', 'c', 'i', 'r', 'c', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_584[] = {
  0x0174
};
PRUnichar nsHtml5NamedCharacters::NAME_585[] = {
  'W', 'e', 'd', 'g', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_585[] = {
  0x22c0
};
PRUnichar nsHtml5NamedCharacters::NAME_586[] = {
  'W', 'f', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_586[] = {
  0xd835, 0xdd1a
};
PRUnichar nsHtml5NamedCharacters::NAME_587[] = {
  'W', 'o', 'p', 'f', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_587[] = {
  0xd835, 0xdd4e
};
PRUnichar nsHtml5NamedCharacters::NAME_588[] = {
  'W', 's', 'c', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_588[] = {
  0xd835, 0xdcb2
};
PRUnichar nsHtml5NamedCharacters::NAME_589[] = {
  'X', 'f', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_589[] = {
  0xd835, 0xdd1b
};
PRUnichar nsHtml5NamedCharacters::NAME_590[] = {
  'X', 'i', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_590[] = {
  0x039e
};
PRUnichar nsHtml5NamedCharacters::NAME_591[] = {
  'X', 'o', 'p', 'f', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_591[] = {
  0xd835, 0xdd4f
};
PRUnichar nsHtml5NamedCharacters::NAME_592[] = {
  'X', 's', 'c', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_592[] = {
  0xd835, 0xdcb3
};
PRUnichar nsHtml5NamedCharacters::NAME_593[] = {
  'Y', 'A', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_593[] = {
  0x042f
};
PRUnichar nsHtml5NamedCharacters::NAME_594[] = {
  'Y', 'I', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_594[] = {
  0x0407
};
PRUnichar nsHtml5NamedCharacters::NAME_595[] = {
  'Y', 'U', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_595[] = {
  0x042e
};
PRUnichar nsHtml5NamedCharacters::NAME_596[] = {
  'Y', 'a', 'c', 'u', 't', 'e'
};
PRUnichar nsHtml5NamedCharacters::VALUE_596[] = {
  0x00dd
};
PRUnichar nsHtml5NamedCharacters::NAME_597[] = {
  'Y', 'a', 'c', 'u', 't', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_597[] = {
  0x00dd
};
PRUnichar nsHtml5NamedCharacters::NAME_598[] = {
  'Y', 'c', 'i', 'r', 'c', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_598[] = {
  0x0176
};
PRUnichar nsHtml5NamedCharacters::NAME_599[] = {
  'Y', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_599[] = {
  0x042b
};
PRUnichar nsHtml5NamedCharacters::NAME_600[] = {
  'Y', 'f', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_600[] = {
  0xd835, 0xdd1c
};
PRUnichar nsHtml5NamedCharacters::NAME_601[] = {
  'Y', 'o', 'p', 'f', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_601[] = {
  0xd835, 0xdd50
};
PRUnichar nsHtml5NamedCharacters::NAME_602[] = {
  'Y', 's', 'c', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_602[] = {
  0xd835, 0xdcb4
};
PRUnichar nsHtml5NamedCharacters::NAME_603[] = {
  'Y', 'u', 'm', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_603[] = {
  0x0178
};
PRUnichar nsHtml5NamedCharacters::NAME_604[] = {
  'Z', 'H', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_604[] = {
  0x0416
};
PRUnichar nsHtml5NamedCharacters::NAME_605[] = {
  'Z', 'a', 'c', 'u', 't', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_605[] = {
  0x0179
};
PRUnichar nsHtml5NamedCharacters::NAME_606[] = {
  'Z', 'c', 'a', 'r', 'o', 'n', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_606[] = {
  0x017d
};
PRUnichar nsHtml5NamedCharacters::NAME_607[] = {
  'Z', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_607[] = {
  0x0417
};
PRUnichar nsHtml5NamedCharacters::NAME_608[] = {
  'Z', 'd', 'o', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_608[] = {
  0x017b
};
PRUnichar nsHtml5NamedCharacters::NAME_609[] = {
  'Z', 'e', 'r', 'o', 'W', 'i', 'd', 't', 'h', 'S', 'p', 'a', 'c', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_609[] = {
  0x200b
};
PRUnichar nsHtml5NamedCharacters::NAME_610[] = {
  'Z', 'e', 't', 'a', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_610[] = {
  0x0396
};
PRUnichar nsHtml5NamedCharacters::NAME_611[] = {
  'Z', 'f', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_611[] = {
  0x2128
};
PRUnichar nsHtml5NamedCharacters::NAME_612[] = {
  'Z', 'o', 'p', 'f', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_612[] = {
  0x2124
};
PRUnichar nsHtml5NamedCharacters::NAME_613[] = {
  'Z', 's', 'c', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_613[] = {
  0xd835, 0xdcb5
};
PRUnichar nsHtml5NamedCharacters::NAME_614[] = {
  'a', 'a', 'c', 'u', 't', 'e'
};
PRUnichar nsHtml5NamedCharacters::VALUE_614[] = {
  0x00e1
};
PRUnichar nsHtml5NamedCharacters::NAME_615[] = {
  'a', 'a', 'c', 'u', 't', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_615[] = {
  0x00e1
};
PRUnichar nsHtml5NamedCharacters::NAME_616[] = {
  'a', 'b', 'r', 'e', 'v', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_616[] = {
  0x0103
};
PRUnichar nsHtml5NamedCharacters::NAME_617[] = {
  'a', 'c', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_617[] = {
  0x223e
};
PRUnichar nsHtml5NamedCharacters::NAME_618[] = {
  'a', 'c', 'd', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_618[] = {
  0x223f
};
PRUnichar nsHtml5NamedCharacters::NAME_619[] = {
  'a', 'c', 'i', 'r', 'c'
};
PRUnichar nsHtml5NamedCharacters::VALUE_619[] = {
  0x00e2
};
PRUnichar nsHtml5NamedCharacters::NAME_620[] = {
  'a', 'c', 'i', 'r', 'c', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_620[] = {
  0x00e2
};
PRUnichar nsHtml5NamedCharacters::NAME_621[] = {
  'a', 'c', 'u', 't', 'e'
};
PRUnichar nsHtml5NamedCharacters::VALUE_621[] = {
  0x00b4
};
PRUnichar nsHtml5NamedCharacters::NAME_622[] = {
  'a', 'c', 'u', 't', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_622[] = {
  0x00b4
};
PRUnichar nsHtml5NamedCharacters::NAME_623[] = {
  'a', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_623[] = {
  0x0430
};
PRUnichar nsHtml5NamedCharacters::NAME_624[] = {
  'a', 'e', 'l', 'i', 'g'
};
PRUnichar nsHtml5NamedCharacters::VALUE_624[] = {
  0x00e6
};
PRUnichar nsHtml5NamedCharacters::NAME_625[] = {
  'a', 'e', 'l', 'i', 'g', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_625[] = {
  0x00e6
};
PRUnichar nsHtml5NamedCharacters::NAME_626[] = {
  'a', 'f', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_626[] = {
  0x2061
};
PRUnichar nsHtml5NamedCharacters::NAME_627[] = {
  'a', 'f', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_627[] = {
  0xd835, 0xdd1e
};
PRUnichar nsHtml5NamedCharacters::NAME_628[] = {
  'a', 'g', 'r', 'a', 'v', 'e'
};
PRUnichar nsHtml5NamedCharacters::VALUE_628[] = {
  0x00e0
};
PRUnichar nsHtml5NamedCharacters::NAME_629[] = {
  'a', 'g', 'r', 'a', 'v', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_629[] = {
  0x00e0
};
PRUnichar nsHtml5NamedCharacters::NAME_630[] = {
  'a', 'l', 'e', 'f', 's', 'y', 'm', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_630[] = {
  0x2135
};
PRUnichar nsHtml5NamedCharacters::NAME_631[] = {
  'a', 'l', 'e', 'p', 'h', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_631[] = {
  0x2135
};
PRUnichar nsHtml5NamedCharacters::NAME_632[] = {
  'a', 'l', 'p', 'h', 'a', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_632[] = {
  0x03b1
};
PRUnichar nsHtml5NamedCharacters::NAME_633[] = {
  'a', 'm', 'a', 'c', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_633[] = {
  0x0101
};
PRUnichar nsHtml5NamedCharacters::NAME_634[] = {
  'a', 'm', 'a', 'l', 'g', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_634[] = {
  0x2a3f
};
PRUnichar nsHtml5NamedCharacters::NAME_635[] = {
  'a', 'm', 'p'
};
PRUnichar nsHtml5NamedCharacters::VALUE_635[] = {
  0x0026
};
PRUnichar nsHtml5NamedCharacters::NAME_636[] = {
  'a', 'm', 'p', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_636[] = {
  0x0026
};
PRUnichar nsHtml5NamedCharacters::NAME_637[] = {
  'a', 'n', 'd', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_637[] = {
  0x2227
};
PRUnichar nsHtml5NamedCharacters::NAME_638[] = {
  'a', 'n', 'd', 'a', 'n', 'd', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_638[] = {
  0x2a55
};
PRUnichar nsHtml5NamedCharacters::NAME_639[] = {
  'a', 'n', 'd', 'd', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_639[] = {
  0x2a5c
};
PRUnichar nsHtml5NamedCharacters::NAME_640[] = {
  'a', 'n', 'd', 's', 'l', 'o', 'p', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_640[] = {
  0x2a58
};
PRUnichar nsHtml5NamedCharacters::NAME_641[] = {
  'a', 'n', 'd', 'v', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_641[] = {
  0x2a5a
};
PRUnichar nsHtml5NamedCharacters::NAME_642[] = {
  'a', 'n', 'g', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_642[] = {
  0x2220
};
PRUnichar nsHtml5NamedCharacters::NAME_643[] = {
  'a', 'n', 'g', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_643[] = {
  0x29a4
};
PRUnichar nsHtml5NamedCharacters::NAME_644[] = {
  'a', 'n', 'g', 'l', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_644[] = {
  0x2220
};
PRUnichar nsHtml5NamedCharacters::NAME_645[] = {
  'a', 'n', 'g', 'm', 's', 'd', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_645[] = {
  0x2221
};
PRUnichar nsHtml5NamedCharacters::NAME_646[] = {
  'a', 'n', 'g', 'm', 's', 'd', 'a', 'a', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_646[] = {
  0x29a8
};
PRUnichar nsHtml5NamedCharacters::NAME_647[] = {
  'a', 'n', 'g', 'm', 's', 'd', 'a', 'b', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_647[] = {
  0x29a9
};
PRUnichar nsHtml5NamedCharacters::NAME_648[] = {
  'a', 'n', 'g', 'm', 's', 'd', 'a', 'c', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_648[] = {
  0x29aa
};
PRUnichar nsHtml5NamedCharacters::NAME_649[] = {
  'a', 'n', 'g', 'm', 's', 'd', 'a', 'd', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_649[] = {
  0x29ab
};
PRUnichar nsHtml5NamedCharacters::NAME_650[] = {
  'a', 'n', 'g', 'm', 's', 'd', 'a', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_650[] = {
  0x29ac
};
PRUnichar nsHtml5NamedCharacters::NAME_651[] = {
  'a', 'n', 'g', 'm', 's', 'd', 'a', 'f', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_651[] = {
  0x29ad
};
PRUnichar nsHtml5NamedCharacters::NAME_652[] = {
  'a', 'n', 'g', 'm', 's', 'd', 'a', 'g', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_652[] = {
  0x29ae
};
PRUnichar nsHtml5NamedCharacters::NAME_653[] = {
  'a', 'n', 'g', 'm', 's', 'd', 'a', 'h', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_653[] = {
  0x29af
};
PRUnichar nsHtml5NamedCharacters::NAME_654[] = {
  'a', 'n', 'g', 'r', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_654[] = {
  0x221f
};
PRUnichar nsHtml5NamedCharacters::NAME_655[] = {
  'a', 'n', 'g', 'r', 't', 'v', 'b', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_655[] = {
  0x22be
};
PRUnichar nsHtml5NamedCharacters::NAME_656[] = {
  'a', 'n', 'g', 'r', 't', 'v', 'b', 'd', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_656[] = {
  0x299d
};
PRUnichar nsHtml5NamedCharacters::NAME_657[] = {
  'a', 'n', 'g', 's', 'p', 'h', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_657[] = {
  0x2222
};
PRUnichar nsHtml5NamedCharacters::NAME_658[] = {
  'a', 'n', 'g', 's', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_658[] = {
  0x212b
};
PRUnichar nsHtml5NamedCharacters::NAME_659[] = {
  'a', 'n', 'g', 'z', 'a', 'r', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_659[] = {
  0x237c
};
PRUnichar nsHtml5NamedCharacters::NAME_660[] = {
  'a', 'o', 'g', 'o', 'n', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_660[] = {
  0x0105
};
PRUnichar nsHtml5NamedCharacters::NAME_661[] = {
  'a', 'o', 'p', 'f', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_661[] = {
  0xd835, 0xdd52
};
PRUnichar nsHtml5NamedCharacters::NAME_662[] = {
  'a', 'p', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_662[] = {
  0x2248
};
PRUnichar nsHtml5NamedCharacters::NAME_663[] = {
  'a', 'p', 'E', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_663[] = {
  0x2a70
};
PRUnichar nsHtml5NamedCharacters::NAME_664[] = {
  'a', 'p', 'a', 'c', 'i', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_664[] = {
  0x2a6f
};
PRUnichar nsHtml5NamedCharacters::NAME_665[] = {
  'a', 'p', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_665[] = {
  0x224a
};
PRUnichar nsHtml5NamedCharacters::NAME_666[] = {
  'a', 'p', 'i', 'd', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_666[] = {
  0x224b
};
PRUnichar nsHtml5NamedCharacters::NAME_667[] = {
  'a', 'p', 'o', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_667[] = {
  0x0027
};
PRUnichar nsHtml5NamedCharacters::NAME_668[] = {
  'a', 'p', 'p', 'r', 'o', 'x', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_668[] = {
  0x2248
};
PRUnichar nsHtml5NamedCharacters::NAME_669[] = {
  'a', 'p', 'p', 'r', 'o', 'x', 'e', 'q', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_669[] = {
  0x224a
};
PRUnichar nsHtml5NamedCharacters::NAME_670[] = {
  'a', 'r', 'i', 'n', 'g'
};
PRUnichar nsHtml5NamedCharacters::VALUE_670[] = {
  0x00e5
};
PRUnichar nsHtml5NamedCharacters::NAME_671[] = {
  'a', 'r', 'i', 'n', 'g', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_671[] = {
  0x00e5
};
PRUnichar nsHtml5NamedCharacters::NAME_672[] = {
  'a', 's', 'c', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_672[] = {
  0xd835, 0xdcb6
};
PRUnichar nsHtml5NamedCharacters::NAME_673[] = {
  'a', 's', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_673[] = {
  0x002a
};
PRUnichar nsHtml5NamedCharacters::NAME_674[] = {
  'a', 's', 'y', 'm', 'p', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_674[] = {
  0x2248
};
PRUnichar nsHtml5NamedCharacters::NAME_675[] = {
  'a', 's', 'y', 'm', 'p', 'e', 'q', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_675[] = {
  0x224d
};
PRUnichar nsHtml5NamedCharacters::NAME_676[] = {
  'a', 't', 'i', 'l', 'd', 'e'
};
PRUnichar nsHtml5NamedCharacters::VALUE_676[] = {
  0x00e3
};
PRUnichar nsHtml5NamedCharacters::NAME_677[] = {
  'a', 't', 'i', 'l', 'd', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_677[] = {
  0x00e3
};
PRUnichar nsHtml5NamedCharacters::NAME_678[] = {
  'a', 'u', 'm', 'l'
};
PRUnichar nsHtml5NamedCharacters::VALUE_678[] = {
  0x00e4
};
PRUnichar nsHtml5NamedCharacters::NAME_679[] = {
  'a', 'u', 'm', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_679[] = {
  0x00e4
};
PRUnichar nsHtml5NamedCharacters::NAME_680[] = {
  'a', 'w', 'c', 'o', 'n', 'i', 'n', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_680[] = {
  0x2233
};
PRUnichar nsHtml5NamedCharacters::NAME_681[] = {
  'a', 'w', 'i', 'n', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_681[] = {
  0x2a11
};
PRUnichar nsHtml5NamedCharacters::NAME_682[] = {
  'b', 'N', 'o', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_682[] = {
  0x2aed
};
PRUnichar nsHtml5NamedCharacters::NAME_683[] = {
  'b', 'a', 'c', 'k', 'c', 'o', 'n', 'g', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_683[] = {
  0x224c
};
PRUnichar nsHtml5NamedCharacters::NAME_684[] = {
  'b', 'a', 'c', 'k', 'e', 'p', 's', 'i', 'l', 'o', 'n', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_684[] = {
  0x03f6
};
PRUnichar nsHtml5NamedCharacters::NAME_685[] = {
  'b', 'a', 'c', 'k', 'p', 'r', 'i', 'm', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_685[] = {
  0x2035
};
PRUnichar nsHtml5NamedCharacters::NAME_686[] = {
  'b', 'a', 'c', 'k', 's', 'i', 'm', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_686[] = {
  0x223d
};
PRUnichar nsHtml5NamedCharacters::NAME_687[] = {
  'b', 'a', 'c', 'k', 's', 'i', 'm', 'e', 'q', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_687[] = {
  0x22cd
};
PRUnichar nsHtml5NamedCharacters::NAME_688[] = {
  'b', 'a', 'r', 'v', 'e', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_688[] = {
  0x22bd
};
PRUnichar nsHtml5NamedCharacters::NAME_689[] = {
  'b', 'a', 'r', 'w', 'e', 'd', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_689[] = {
  0x2305
};
PRUnichar nsHtml5NamedCharacters::NAME_690[] = {
  'b', 'a', 'r', 'w', 'e', 'd', 'g', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_690[] = {
  0x2305
};
PRUnichar nsHtml5NamedCharacters::NAME_691[] = {
  'b', 'b', 'r', 'k', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_691[] = {
  0x23b5
};
PRUnichar nsHtml5NamedCharacters::NAME_692[] = {
  'b', 'b', 'r', 'k', 't', 'b', 'r', 'k', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_692[] = {
  0x23b6
};
PRUnichar nsHtml5NamedCharacters::NAME_693[] = {
  'b', 'c', 'o', 'n', 'g', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_693[] = {
  0x224c
};
PRUnichar nsHtml5NamedCharacters::NAME_694[] = {
  'b', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_694[] = {
  0x0431
};
PRUnichar nsHtml5NamedCharacters::NAME_695[] = {
  'b', 'd', 'q', 'u', 'o', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_695[] = {
  0x201e
};
PRUnichar nsHtml5NamedCharacters::NAME_696[] = {
  'b', 'e', 'c', 'a', 'u', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_696[] = {
  0x2235
};
PRUnichar nsHtml5NamedCharacters::NAME_697[] = {
  'b', 'e', 'c', 'a', 'u', 's', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_697[] = {
  0x2235
};
PRUnichar nsHtml5NamedCharacters::NAME_698[] = {
  'b', 'e', 'm', 'p', 't', 'y', 'v', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_698[] = {
  0x29b0
};
PRUnichar nsHtml5NamedCharacters::NAME_699[] = {
  'b', 'e', 'p', 's', 'i', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_699[] = {
  0x03f6
};
PRUnichar nsHtml5NamedCharacters::NAME_700[] = {
  'b', 'e', 'r', 'n', 'o', 'u', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_700[] = {
  0x212c
};
PRUnichar nsHtml5NamedCharacters::NAME_701[] = {
  'b', 'e', 't', 'a', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_701[] = {
  0x03b2
};
PRUnichar nsHtml5NamedCharacters::NAME_702[] = {
  'b', 'e', 't', 'h', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_702[] = {
  0x2136
};
PRUnichar nsHtml5NamedCharacters::NAME_703[] = {
  'b', 'e', 't', 'w', 'e', 'e', 'n', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_703[] = {
  0x226c
};
PRUnichar nsHtml5NamedCharacters::NAME_704[] = {
  'b', 'f', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_704[] = {
  0xd835, 0xdd1f
};
PRUnichar nsHtml5NamedCharacters::NAME_705[] = {
  'b', 'i', 'g', 'c', 'a', 'p', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_705[] = {
  0x22c2
};
PRUnichar nsHtml5NamedCharacters::NAME_706[] = {
  'b', 'i', 'g', 'c', 'i', 'r', 'c', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_706[] = {
  0x25ef
};
PRUnichar nsHtml5NamedCharacters::NAME_707[] = {
  'b', 'i', 'g', 'c', 'u', 'p', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_707[] = {
  0x22c3
};
PRUnichar nsHtml5NamedCharacters::NAME_708[] = {
  'b', 'i', 'g', 'o', 'd', 'o', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_708[] = {
  0x2a00
};
PRUnichar nsHtml5NamedCharacters::NAME_709[] = {
  'b', 'i', 'g', 'o', 'p', 'l', 'u', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_709[] = {
  0x2a01
};
PRUnichar nsHtml5NamedCharacters::NAME_710[] = {
  'b', 'i', 'g', 'o', 't', 'i', 'm', 'e', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_710[] = {
  0x2a02
};
PRUnichar nsHtml5NamedCharacters::NAME_711[] = {
  'b', 'i', 'g', 's', 'q', 'c', 'u', 'p', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_711[] = {
  0x2a06
};
PRUnichar nsHtml5NamedCharacters::NAME_712[] = {
  'b', 'i', 'g', 's', 't', 'a', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_712[] = {
  0x2605
};
PRUnichar nsHtml5NamedCharacters::NAME_713[] = {
  'b', 'i', 'g', 't', 'r', 'i', 'a', 'n', 'g', 'l', 'e', 'd', 'o', 'w', 'n', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_713[] = {
  0x25bd
};
PRUnichar nsHtml5NamedCharacters::NAME_714[] = {
  'b', 'i', 'g', 't', 'r', 'i', 'a', 'n', 'g', 'l', 'e', 'u', 'p', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_714[] = {
  0x25b3
};
PRUnichar nsHtml5NamedCharacters::NAME_715[] = {
  'b', 'i', 'g', 'u', 'p', 'l', 'u', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_715[] = {
  0x2a04
};
PRUnichar nsHtml5NamedCharacters::NAME_716[] = {
  'b', 'i', 'g', 'v', 'e', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_716[] = {
  0x22c1
};
PRUnichar nsHtml5NamedCharacters::NAME_717[] = {
  'b', 'i', 'g', 'w', 'e', 'd', 'g', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_717[] = {
  0x22c0
};
PRUnichar nsHtml5NamedCharacters::NAME_718[] = {
  'b', 'k', 'a', 'r', 'o', 'w', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_718[] = {
  0x290d
};
PRUnichar nsHtml5NamedCharacters::NAME_719[] = {
  'b', 'l', 'a', 'c', 'k', 'l', 'o', 'z', 'e', 'n', 'g', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_719[] = {
  0x29eb
};
PRUnichar nsHtml5NamedCharacters::NAME_720[] = {
  'b', 'l', 'a', 'c', 'k', 's', 'q', 'u', 'a', 'r', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_720[] = {
  0x25aa
};
PRUnichar nsHtml5NamedCharacters::NAME_721[] = {
  'b', 'l', 'a', 'c', 'k', 't', 'r', 'i', 'a', 'n', 'g', 'l', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_721[] = {
  0x25b4
};
PRUnichar nsHtml5NamedCharacters::NAME_722[] = {
  'b', 'l', 'a', 'c', 'k', 't', 'r', 'i', 'a', 'n', 'g', 'l', 'e', 'd', 'o', 'w', 'n', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_722[] = {
  0x25be
};
PRUnichar nsHtml5NamedCharacters::NAME_723[] = {
  'b', 'l', 'a', 'c', 'k', 't', 'r', 'i', 'a', 'n', 'g', 'l', 'e', 'l', 'e', 'f', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_723[] = {
  0x25c2
};
PRUnichar nsHtml5NamedCharacters::NAME_724[] = {
  'b', 'l', 'a', 'c', 'k', 't', 'r', 'i', 'a', 'n', 'g', 'l', 'e', 'r', 'i', 'g', 'h', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_724[] = {
  0x25b8
};
PRUnichar nsHtml5NamedCharacters::NAME_725[] = {
  'b', 'l', 'a', 'n', 'k', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_725[] = {
  0x2423
};
PRUnichar nsHtml5NamedCharacters::NAME_726[] = {
  'b', 'l', 'k', '1', '2', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_726[] = {
  0x2592
};
PRUnichar nsHtml5NamedCharacters::NAME_727[] = {
  'b', 'l', 'k', '1', '4', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_727[] = {
  0x2591
};
PRUnichar nsHtml5NamedCharacters::NAME_728[] = {
  'b', 'l', 'k', '3', '4', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_728[] = {
  0x2593
};
PRUnichar nsHtml5NamedCharacters::NAME_729[] = {
  'b', 'l', 'o', 'c', 'k', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_729[] = {
  0x2588
};
PRUnichar nsHtml5NamedCharacters::NAME_730[] = {
  'b', 'n', 'o', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_730[] = {
  0x2310
};
PRUnichar nsHtml5NamedCharacters::NAME_731[] = {
  'b', 'o', 'p', 'f', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_731[] = {
  0xd835, 0xdd53
};
PRUnichar nsHtml5NamedCharacters::NAME_732[] = {
  'b', 'o', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_732[] = {
  0x22a5
};
PRUnichar nsHtml5NamedCharacters::NAME_733[] = {
  'b', 'o', 't', 't', 'o', 'm', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_733[] = {
  0x22a5
};
PRUnichar nsHtml5NamedCharacters::NAME_734[] = {
  'b', 'o', 'w', 't', 'i', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_734[] = {
  0x22c8
};
PRUnichar nsHtml5NamedCharacters::NAME_735[] = {
  'b', 'o', 'x', 'D', 'L', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_735[] = {
  0x2557
};
PRUnichar nsHtml5NamedCharacters::NAME_736[] = {
  'b', 'o', 'x', 'D', 'R', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_736[] = {
  0x2554
};
PRUnichar nsHtml5NamedCharacters::NAME_737[] = {
  'b', 'o', 'x', 'D', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_737[] = {
  0x2556
};
PRUnichar nsHtml5NamedCharacters::NAME_738[] = {
  'b', 'o', 'x', 'D', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_738[] = {
  0x2553
};
PRUnichar nsHtml5NamedCharacters::NAME_739[] = {
  'b', 'o', 'x', 'H', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_739[] = {
  0x2550
};
PRUnichar nsHtml5NamedCharacters::NAME_740[] = {
  'b', 'o', 'x', 'H', 'D', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_740[] = {
  0x2566
};
PRUnichar nsHtml5NamedCharacters::NAME_741[] = {
  'b', 'o', 'x', 'H', 'U', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_741[] = {
  0x2569
};
PRUnichar nsHtml5NamedCharacters::NAME_742[] = {
  'b', 'o', 'x', 'H', 'd', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_742[] = {
  0x2564
};
PRUnichar nsHtml5NamedCharacters::NAME_743[] = {
  'b', 'o', 'x', 'H', 'u', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_743[] = {
  0x2567
};
PRUnichar nsHtml5NamedCharacters::NAME_744[] = {
  'b', 'o', 'x', 'U', 'L', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_744[] = {
  0x255d
};
PRUnichar nsHtml5NamedCharacters::NAME_745[] = {
  'b', 'o', 'x', 'U', 'R', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_745[] = {
  0x255a
};
PRUnichar nsHtml5NamedCharacters::NAME_746[] = {
  'b', 'o', 'x', 'U', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_746[] = {
  0x255c
};
PRUnichar nsHtml5NamedCharacters::NAME_747[] = {
  'b', 'o', 'x', 'U', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_747[] = {
  0x2559
};
PRUnichar nsHtml5NamedCharacters::NAME_748[] = {
  'b', 'o', 'x', 'V', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_748[] = {
  0x2551
};
PRUnichar nsHtml5NamedCharacters::NAME_749[] = {
  'b', 'o', 'x', 'V', 'H', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_749[] = {
  0x256c
};
PRUnichar nsHtml5NamedCharacters::NAME_750[] = {
  'b', 'o', 'x', 'V', 'L', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_750[] = {
  0x2563
};
PRUnichar nsHtml5NamedCharacters::NAME_751[] = {
  'b', 'o', 'x', 'V', 'R', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_751[] = {
  0x2560
};
PRUnichar nsHtml5NamedCharacters::NAME_752[] = {
  'b', 'o', 'x', 'V', 'h', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_752[] = {
  0x256b
};
PRUnichar nsHtml5NamedCharacters::NAME_753[] = {
  'b', 'o', 'x', 'V', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_753[] = {
  0x2562
};
PRUnichar nsHtml5NamedCharacters::NAME_754[] = {
  'b', 'o', 'x', 'V', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_754[] = {
  0x255f
};
PRUnichar nsHtml5NamedCharacters::NAME_755[] = {
  'b', 'o', 'x', 'b', 'o', 'x', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_755[] = {
  0x29c9
};
PRUnichar nsHtml5NamedCharacters::NAME_756[] = {
  'b', 'o', 'x', 'd', 'L', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_756[] = {
  0x2555
};
PRUnichar nsHtml5NamedCharacters::NAME_757[] = {
  'b', 'o', 'x', 'd', 'R', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_757[] = {
  0x2552
};
PRUnichar nsHtml5NamedCharacters::NAME_758[] = {
  'b', 'o', 'x', 'd', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_758[] = {
  0x2510
};
PRUnichar nsHtml5NamedCharacters::NAME_759[] = {
  'b', 'o', 'x', 'd', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_759[] = {
  0x250c
};
PRUnichar nsHtml5NamedCharacters::NAME_760[] = {
  'b', 'o', 'x', 'h', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_760[] = {
  0x2500
};
PRUnichar nsHtml5NamedCharacters::NAME_761[] = {
  'b', 'o', 'x', 'h', 'D', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_761[] = {
  0x2565
};
PRUnichar nsHtml5NamedCharacters::NAME_762[] = {
  'b', 'o', 'x', 'h', 'U', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_762[] = {
  0x2568
};
PRUnichar nsHtml5NamedCharacters::NAME_763[] = {
  'b', 'o', 'x', 'h', 'd', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_763[] = {
  0x252c
};
PRUnichar nsHtml5NamedCharacters::NAME_764[] = {
  'b', 'o', 'x', 'h', 'u', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_764[] = {
  0x2534
};
PRUnichar nsHtml5NamedCharacters::NAME_765[] = {
  'b', 'o', 'x', 'm', 'i', 'n', 'u', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_765[] = {
  0x229f
};
PRUnichar nsHtml5NamedCharacters::NAME_766[] = {
  'b', 'o', 'x', 'p', 'l', 'u', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_766[] = {
  0x229e
};
PRUnichar nsHtml5NamedCharacters::NAME_767[] = {
  'b', 'o', 'x', 't', 'i', 'm', 'e', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_767[] = {
  0x22a0
};
PRUnichar nsHtml5NamedCharacters::NAME_768[] = {
  'b', 'o', 'x', 'u', 'L', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_768[] = {
  0x255b
};
PRUnichar nsHtml5NamedCharacters::NAME_769[] = {
  'b', 'o', 'x', 'u', 'R', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_769[] = {
  0x2558
};
PRUnichar nsHtml5NamedCharacters::NAME_770[] = {
  'b', 'o', 'x', 'u', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_770[] = {
  0x2518
};
PRUnichar nsHtml5NamedCharacters::NAME_771[] = {
  'b', 'o', 'x', 'u', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_771[] = {
  0x2514
};
PRUnichar nsHtml5NamedCharacters::NAME_772[] = {
  'b', 'o', 'x', 'v', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_772[] = {
  0x2502
};
PRUnichar nsHtml5NamedCharacters::NAME_773[] = {
  'b', 'o', 'x', 'v', 'H', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_773[] = {
  0x256a
};
PRUnichar nsHtml5NamedCharacters::NAME_774[] = {
  'b', 'o', 'x', 'v', 'L', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_774[] = {
  0x2561
};
PRUnichar nsHtml5NamedCharacters::NAME_775[] = {
  'b', 'o', 'x', 'v', 'R', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_775[] = {
  0x255e
};
PRUnichar nsHtml5NamedCharacters::NAME_776[] = {
  'b', 'o', 'x', 'v', 'h', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_776[] = {
  0x253c
};
PRUnichar nsHtml5NamedCharacters::NAME_777[] = {
  'b', 'o', 'x', 'v', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_777[] = {
  0x2524
};
PRUnichar nsHtml5NamedCharacters::NAME_778[] = {
  'b', 'o', 'x', 'v', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_778[] = {
  0x251c
};
PRUnichar nsHtml5NamedCharacters::NAME_779[] = {
  'b', 'p', 'r', 'i', 'm', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_779[] = {
  0x2035
};
PRUnichar nsHtml5NamedCharacters::NAME_780[] = {
  'b', 'r', 'e', 'v', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_780[] = {
  0x02d8
};
PRUnichar nsHtml5NamedCharacters::NAME_781[] = {
  'b', 'r', 'v', 'b', 'a', 'r'
};
PRUnichar nsHtml5NamedCharacters::VALUE_781[] = {
  0x00a6
};
PRUnichar nsHtml5NamedCharacters::NAME_782[] = {
  'b', 'r', 'v', 'b', 'a', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_782[] = {
  0x00a6
};
PRUnichar nsHtml5NamedCharacters::NAME_783[] = {
  'b', 's', 'c', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_783[] = {
  0xd835, 0xdcb7
};
PRUnichar nsHtml5NamedCharacters::NAME_784[] = {
  'b', 's', 'e', 'm', 'i', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_784[] = {
  0x204f
};
PRUnichar nsHtml5NamedCharacters::NAME_785[] = {
  'b', 's', 'i', 'm', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_785[] = {
  0x223d
};
PRUnichar nsHtml5NamedCharacters::NAME_786[] = {
  'b', 's', 'i', 'm', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_786[] = {
  0x22cd
};
PRUnichar nsHtml5NamedCharacters::NAME_787[] = {
  'b', 's', 'o', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_787[] = {
  0x005c
};
PRUnichar nsHtml5NamedCharacters::NAME_788[] = {
  'b', 's', 'o', 'l', 'b', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_788[] = {
  0x29c5
};
PRUnichar nsHtml5NamedCharacters::NAME_789[] = {
  'b', 'u', 'l', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_789[] = {
  0x2022
};
PRUnichar nsHtml5NamedCharacters::NAME_790[] = {
  'b', 'u', 'l', 'l', 'e', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_790[] = {
  0x2022
};
PRUnichar nsHtml5NamedCharacters::NAME_791[] = {
  'b', 'u', 'm', 'p', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_791[] = {
  0x224e
};
PRUnichar nsHtml5NamedCharacters::NAME_792[] = {
  'b', 'u', 'm', 'p', 'E', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_792[] = {
  0x2aae
};
PRUnichar nsHtml5NamedCharacters::NAME_793[] = {
  'b', 'u', 'm', 'p', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_793[] = {
  0x224f
};
PRUnichar nsHtml5NamedCharacters::NAME_794[] = {
  'b', 'u', 'm', 'p', 'e', 'q', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_794[] = {
  0x224f
};
PRUnichar nsHtml5NamedCharacters::NAME_795[] = {
  'c', 'a', 'c', 'u', 't', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_795[] = {
  0x0107
};
PRUnichar nsHtml5NamedCharacters::NAME_796[] = {
  'c', 'a', 'p', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_796[] = {
  0x2229
};
PRUnichar nsHtml5NamedCharacters::NAME_797[] = {
  'c', 'a', 'p', 'a', 'n', 'd', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_797[] = {
  0x2a44
};
PRUnichar nsHtml5NamedCharacters::NAME_798[] = {
  'c', 'a', 'p', 'b', 'r', 'c', 'u', 'p', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_798[] = {
  0x2a49
};
PRUnichar nsHtml5NamedCharacters::NAME_799[] = {
  'c', 'a', 'p', 'c', 'a', 'p', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_799[] = {
  0x2a4b
};
PRUnichar nsHtml5NamedCharacters::NAME_800[] = {
  'c', 'a', 'p', 'c', 'u', 'p', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_800[] = {
  0x2a47
};
PRUnichar nsHtml5NamedCharacters::NAME_801[] = {
  'c', 'a', 'p', 'd', 'o', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_801[] = {
  0x2a40
};
PRUnichar nsHtml5NamedCharacters::NAME_802[] = {
  'c', 'a', 'r', 'e', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_802[] = {
  0x2041
};
PRUnichar nsHtml5NamedCharacters::NAME_803[] = {
  'c', 'a', 'r', 'o', 'n', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_803[] = {
  0x02c7
};
PRUnichar nsHtml5NamedCharacters::NAME_804[] = {
  'c', 'c', 'a', 'p', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_804[] = {
  0x2a4d
};
PRUnichar nsHtml5NamedCharacters::NAME_805[] = {
  'c', 'c', 'a', 'r', 'o', 'n', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_805[] = {
  0x010d
};
PRUnichar nsHtml5NamedCharacters::NAME_806[] = {
  'c', 'c', 'e', 'd', 'i', 'l'
};
PRUnichar nsHtml5NamedCharacters::VALUE_806[] = {
  0x00e7
};
PRUnichar nsHtml5NamedCharacters::NAME_807[] = {
  'c', 'c', 'e', 'd', 'i', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_807[] = {
  0x00e7
};
PRUnichar nsHtml5NamedCharacters::NAME_808[] = {
  'c', 'c', 'i', 'r', 'c', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_808[] = {
  0x0109
};
PRUnichar nsHtml5NamedCharacters::NAME_809[] = {
  'c', 'c', 'u', 'p', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_809[] = {
  0x2a4c
};
PRUnichar nsHtml5NamedCharacters::NAME_810[] = {
  'c', 'c', 'u', 'p', 's', 's', 'm', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_810[] = {
  0x2a50
};
PRUnichar nsHtml5NamedCharacters::NAME_811[] = {
  'c', 'd', 'o', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_811[] = {
  0x010b
};
PRUnichar nsHtml5NamedCharacters::NAME_812[] = {
  'c', 'e', 'd', 'i', 'l'
};
PRUnichar nsHtml5NamedCharacters::VALUE_812[] = {
  0x00b8
};
PRUnichar nsHtml5NamedCharacters::NAME_813[] = {
  'c', 'e', 'd', 'i', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_813[] = {
  0x00b8
};
PRUnichar nsHtml5NamedCharacters::NAME_814[] = {
  'c', 'e', 'm', 'p', 't', 'y', 'v', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_814[] = {
  0x29b2
};
PRUnichar nsHtml5NamedCharacters::NAME_815[] = {
  'c', 'e', 'n', 't'
};
PRUnichar nsHtml5NamedCharacters::VALUE_815[] = {
  0x00a2
};
PRUnichar nsHtml5NamedCharacters::NAME_816[] = {
  'c', 'e', 'n', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_816[] = {
  0x00a2
};
PRUnichar nsHtml5NamedCharacters::NAME_817[] = {
  'c', 'e', 'n', 't', 'e', 'r', 'd', 'o', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_817[] = {
  0x00b7
};
PRUnichar nsHtml5NamedCharacters::NAME_818[] = {
  'c', 'f', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_818[] = {
  0xd835, 0xdd20
};
PRUnichar nsHtml5NamedCharacters::NAME_819[] = {
  'c', 'h', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_819[] = {
  0x0447
};
PRUnichar nsHtml5NamedCharacters::NAME_820[] = {
  'c', 'h', 'e', 'c', 'k', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_820[] = {
  0x2713
};
PRUnichar nsHtml5NamedCharacters::NAME_821[] = {
  'c', 'h', 'e', 'c', 'k', 'm', 'a', 'r', 'k', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_821[] = {
  0x2713
};
PRUnichar nsHtml5NamedCharacters::NAME_822[] = {
  'c', 'h', 'i', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_822[] = {
  0x03c7
};
PRUnichar nsHtml5NamedCharacters::NAME_823[] = {
  'c', 'i', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_823[] = {
  0x25cb
};
PRUnichar nsHtml5NamedCharacters::NAME_824[] = {
  'c', 'i', 'r', 'E', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_824[] = {
  0x29c3
};
PRUnichar nsHtml5NamedCharacters::NAME_825[] = {
  'c', 'i', 'r', 'c', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_825[] = {
  0x02c6
};
PRUnichar nsHtml5NamedCharacters::NAME_826[] = {
  'c', 'i', 'r', 'c', 'e', 'q', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_826[] = {
  0x2257
};
PRUnichar nsHtml5NamedCharacters::NAME_827[] = {
  'c', 'i', 'r', 'c', 'l', 'e', 'a', 'r', 'r', 'o', 'w', 'l', 'e', 'f', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_827[] = {
  0x21ba
};
PRUnichar nsHtml5NamedCharacters::NAME_828[] = {
  'c', 'i', 'r', 'c', 'l', 'e', 'a', 'r', 'r', 'o', 'w', 'r', 'i', 'g', 'h', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_828[] = {
  0x21bb
};
PRUnichar nsHtml5NamedCharacters::NAME_829[] = {
  'c', 'i', 'r', 'c', 'l', 'e', 'd', 'R', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_829[] = {
  0x00ae
};
PRUnichar nsHtml5NamedCharacters::NAME_830[] = {
  'c', 'i', 'r', 'c', 'l', 'e', 'd', 'S', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_830[] = {
  0x24c8
};
PRUnichar nsHtml5NamedCharacters::NAME_831[] = {
  'c', 'i', 'r', 'c', 'l', 'e', 'd', 'a', 's', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_831[] = {
  0x229b
};
PRUnichar nsHtml5NamedCharacters::NAME_832[] = {
  'c', 'i', 'r', 'c', 'l', 'e', 'd', 'c', 'i', 'r', 'c', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_832[] = {
  0x229a
};
PRUnichar nsHtml5NamedCharacters::NAME_833[] = {
  'c', 'i', 'r', 'c', 'l', 'e', 'd', 'd', 'a', 's', 'h', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_833[] = {
  0x229d
};
PRUnichar nsHtml5NamedCharacters::NAME_834[] = {
  'c', 'i', 'r', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_834[] = {
  0x2257
};
PRUnichar nsHtml5NamedCharacters::NAME_835[] = {
  'c', 'i', 'r', 'f', 'n', 'i', 'n', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_835[] = {
  0x2a10
};
PRUnichar nsHtml5NamedCharacters::NAME_836[] = {
  'c', 'i', 'r', 'm', 'i', 'd', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_836[] = {
  0x2aef
};
PRUnichar nsHtml5NamedCharacters::NAME_837[] = {
  'c', 'i', 'r', 's', 'c', 'i', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_837[] = {
  0x29c2
};
PRUnichar nsHtml5NamedCharacters::NAME_838[] = {
  'c', 'l', 'u', 'b', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_838[] = {
  0x2663
};
PRUnichar nsHtml5NamedCharacters::NAME_839[] = {
  'c', 'l', 'u', 'b', 's', 'u', 'i', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_839[] = {
  0x2663
};
PRUnichar nsHtml5NamedCharacters::NAME_840[] = {
  'c', 'o', 'l', 'o', 'n', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_840[] = {
  0x003a
};
PRUnichar nsHtml5NamedCharacters::NAME_841[] = {
  'c', 'o', 'l', 'o', 'n', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_841[] = {
  0x2254
};
PRUnichar nsHtml5NamedCharacters::NAME_842[] = {
  'c', 'o', 'l', 'o', 'n', 'e', 'q', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_842[] = {
  0x2254
};
PRUnichar nsHtml5NamedCharacters::NAME_843[] = {
  'c', 'o', 'm', 'm', 'a', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_843[] = {
  0x002c
};
PRUnichar nsHtml5NamedCharacters::NAME_844[] = {
  'c', 'o', 'm', 'm', 'a', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_844[] = {
  0x0040
};
PRUnichar nsHtml5NamedCharacters::NAME_845[] = {
  'c', 'o', 'm', 'p', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_845[] = {
  0x2201
};
PRUnichar nsHtml5NamedCharacters::NAME_846[] = {
  'c', 'o', 'm', 'p', 'f', 'n', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_846[] = {
  0x2218
};
PRUnichar nsHtml5NamedCharacters::NAME_847[] = {
  'c', 'o', 'm', 'p', 'l', 'e', 'm', 'e', 'n', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_847[] = {
  0x2201
};
PRUnichar nsHtml5NamedCharacters::NAME_848[] = {
  'c', 'o', 'm', 'p', 'l', 'e', 'x', 'e', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_848[] = {
  0x2102
};
PRUnichar nsHtml5NamedCharacters::NAME_849[] = {
  'c', 'o', 'n', 'g', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_849[] = {
  0x2245
};
PRUnichar nsHtml5NamedCharacters::NAME_850[] = {
  'c', 'o', 'n', 'g', 'd', 'o', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_850[] = {
  0x2a6d
};
PRUnichar nsHtml5NamedCharacters::NAME_851[] = {
  'c', 'o', 'n', 'i', 'n', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_851[] = {
  0x222e
};
PRUnichar nsHtml5NamedCharacters::NAME_852[] = {
  'c', 'o', 'p', 'f', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_852[] = {
  0xd835, 0xdd54
};
PRUnichar nsHtml5NamedCharacters::NAME_853[] = {
  'c', 'o', 'p', 'r', 'o', 'd', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_853[] = {
  0x2210
};
PRUnichar nsHtml5NamedCharacters::NAME_854[] = {
  'c', 'o', 'p', 'y'
};
PRUnichar nsHtml5NamedCharacters::VALUE_854[] = {
  0x00a9
};
PRUnichar nsHtml5NamedCharacters::NAME_855[] = {
  'c', 'o', 'p', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_855[] = {
  0x00a9
};
PRUnichar nsHtml5NamedCharacters::NAME_856[] = {
  'c', 'o', 'p', 'y', 's', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_856[] = {
  0x2117
};
PRUnichar nsHtml5NamedCharacters::NAME_857[] = {
  'c', 'r', 'a', 'r', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_857[] = {
  0x21b5
};
PRUnichar nsHtml5NamedCharacters::NAME_858[] = {
  'c', 'r', 'o', 's', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_858[] = {
  0x2717
};
PRUnichar nsHtml5NamedCharacters::NAME_859[] = {
  'c', 's', 'c', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_859[] = {
  0xd835, 0xdcb8
};
PRUnichar nsHtml5NamedCharacters::NAME_860[] = {
  'c', 's', 'u', 'b', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_860[] = {
  0x2acf
};
PRUnichar nsHtml5NamedCharacters::NAME_861[] = {
  'c', 's', 'u', 'b', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_861[] = {
  0x2ad1
};
PRUnichar nsHtml5NamedCharacters::NAME_862[] = {
  'c', 's', 'u', 'p', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_862[] = {
  0x2ad0
};
PRUnichar nsHtml5NamedCharacters::NAME_863[] = {
  'c', 's', 'u', 'p', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_863[] = {
  0x2ad2
};
PRUnichar nsHtml5NamedCharacters::NAME_864[] = {
  'c', 't', 'd', 'o', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_864[] = {
  0x22ef
};
PRUnichar nsHtml5NamedCharacters::NAME_865[] = {
  'c', 'u', 'd', 'a', 'r', 'r', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_865[] = {
  0x2938
};
PRUnichar nsHtml5NamedCharacters::NAME_866[] = {
  'c', 'u', 'd', 'a', 'r', 'r', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_866[] = {
  0x2935
};
PRUnichar nsHtml5NamedCharacters::NAME_867[] = {
  'c', 'u', 'e', 'p', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_867[] = {
  0x22de
};
PRUnichar nsHtml5NamedCharacters::NAME_868[] = {
  'c', 'u', 'e', 's', 'c', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_868[] = {
  0x22df
};
PRUnichar nsHtml5NamedCharacters::NAME_869[] = {
  'c', 'u', 'l', 'a', 'r', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_869[] = {
  0x21b6
};
PRUnichar nsHtml5NamedCharacters::NAME_870[] = {
  'c', 'u', 'l', 'a', 'r', 'r', 'p', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_870[] = {
  0x293d
};
PRUnichar nsHtml5NamedCharacters::NAME_871[] = {
  'c', 'u', 'p', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_871[] = {
  0x222a
};
PRUnichar nsHtml5NamedCharacters::NAME_872[] = {
  'c', 'u', 'p', 'b', 'r', 'c', 'a', 'p', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_872[] = {
  0x2a48
};
PRUnichar nsHtml5NamedCharacters::NAME_873[] = {
  'c', 'u', 'p', 'c', 'a', 'p', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_873[] = {
  0x2a46
};
PRUnichar nsHtml5NamedCharacters::NAME_874[] = {
  'c', 'u', 'p', 'c', 'u', 'p', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_874[] = {
  0x2a4a
};
PRUnichar nsHtml5NamedCharacters::NAME_875[] = {
  'c', 'u', 'p', 'd', 'o', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_875[] = {
  0x228d
};
PRUnichar nsHtml5NamedCharacters::NAME_876[] = {
  'c', 'u', 'p', 'o', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_876[] = {
  0x2a45
};
PRUnichar nsHtml5NamedCharacters::NAME_877[] = {
  'c', 'u', 'r', 'a', 'r', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_877[] = {
  0x21b7
};
PRUnichar nsHtml5NamedCharacters::NAME_878[] = {
  'c', 'u', 'r', 'a', 'r', 'r', 'm', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_878[] = {
  0x293c
};
PRUnichar nsHtml5NamedCharacters::NAME_879[] = {
  'c', 'u', 'r', 'l', 'y', 'e', 'q', 'p', 'r', 'e', 'c', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_879[] = {
  0x22de
};
PRUnichar nsHtml5NamedCharacters::NAME_880[] = {
  'c', 'u', 'r', 'l', 'y', 'e', 'q', 's', 'u', 'c', 'c', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_880[] = {
  0x22df
};
PRUnichar nsHtml5NamedCharacters::NAME_881[] = {
  'c', 'u', 'r', 'l', 'y', 'v', 'e', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_881[] = {
  0x22ce
};
PRUnichar nsHtml5NamedCharacters::NAME_882[] = {
  'c', 'u', 'r', 'l', 'y', 'w', 'e', 'd', 'g', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_882[] = {
  0x22cf
};
PRUnichar nsHtml5NamedCharacters::NAME_883[] = {
  'c', 'u', 'r', 'r', 'e', 'n'
};
PRUnichar nsHtml5NamedCharacters::VALUE_883[] = {
  0x00a4
};
PRUnichar nsHtml5NamedCharacters::NAME_884[] = {
  'c', 'u', 'r', 'r', 'e', 'n', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_884[] = {
  0x00a4
};
PRUnichar nsHtml5NamedCharacters::NAME_885[] = {
  'c', 'u', 'r', 'v', 'e', 'a', 'r', 'r', 'o', 'w', 'l', 'e', 'f', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_885[] = {
  0x21b6
};
PRUnichar nsHtml5NamedCharacters::NAME_886[] = {
  'c', 'u', 'r', 'v', 'e', 'a', 'r', 'r', 'o', 'w', 'r', 'i', 'g', 'h', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_886[] = {
  0x21b7
};
PRUnichar nsHtml5NamedCharacters::NAME_887[] = {
  'c', 'u', 'v', 'e', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_887[] = {
  0x22ce
};
PRUnichar nsHtml5NamedCharacters::NAME_888[] = {
  'c', 'u', 'w', 'e', 'd', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_888[] = {
  0x22cf
};
PRUnichar nsHtml5NamedCharacters::NAME_889[] = {
  'c', 'w', 'c', 'o', 'n', 'i', 'n', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_889[] = {
  0x2232
};
PRUnichar nsHtml5NamedCharacters::NAME_890[] = {
  'c', 'w', 'i', 'n', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_890[] = {
  0x2231
};
PRUnichar nsHtml5NamedCharacters::NAME_891[] = {
  'c', 'y', 'l', 'c', 't', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_891[] = {
  0x232d
};
PRUnichar nsHtml5NamedCharacters::NAME_892[] = {
  'd', 'A', 'r', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_892[] = {
  0x21d3
};
PRUnichar nsHtml5NamedCharacters::NAME_893[] = {
  'd', 'H', 'a', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_893[] = {
  0x2965
};
PRUnichar nsHtml5NamedCharacters::NAME_894[] = {
  'd', 'a', 'g', 'g', 'e', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_894[] = {
  0x2020
};
PRUnichar nsHtml5NamedCharacters::NAME_895[] = {
  'd', 'a', 'l', 'e', 't', 'h', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_895[] = {
  0x2138
};
PRUnichar nsHtml5NamedCharacters::NAME_896[] = {
  'd', 'a', 'r', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_896[] = {
  0x2193
};
PRUnichar nsHtml5NamedCharacters::NAME_897[] = {
  'd', 'a', 's', 'h', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_897[] = {
  0x2010
};
PRUnichar nsHtml5NamedCharacters::NAME_898[] = {
  'd', 'a', 's', 'h', 'v', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_898[] = {
  0x22a3
};
PRUnichar nsHtml5NamedCharacters::NAME_899[] = {
  'd', 'b', 'k', 'a', 'r', 'o', 'w', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_899[] = {
  0x290f
};
PRUnichar nsHtml5NamedCharacters::NAME_900[] = {
  'd', 'b', 'l', 'a', 'c', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_900[] = {
  0x02dd
};
PRUnichar nsHtml5NamedCharacters::NAME_901[] = {
  'd', 'c', 'a', 'r', 'o', 'n', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_901[] = {
  0x010f
};
PRUnichar nsHtml5NamedCharacters::NAME_902[] = {
  'd', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_902[] = {
  0x0434
};
PRUnichar nsHtml5NamedCharacters::NAME_903[] = {
  'd', 'd', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_903[] = {
  0x2146
};
PRUnichar nsHtml5NamedCharacters::NAME_904[] = {
  'd', 'd', 'a', 'g', 'g', 'e', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_904[] = {
  0x2021
};
PRUnichar nsHtml5NamedCharacters::NAME_905[] = {
  'd', 'd', 'a', 'r', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_905[] = {
  0x21ca
};
PRUnichar nsHtml5NamedCharacters::NAME_906[] = {
  'd', 'd', 'o', 't', 's', 'e', 'q', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_906[] = {
  0x2a77
};
PRUnichar nsHtml5NamedCharacters::NAME_907[] = {
  'd', 'e', 'g'
};
PRUnichar nsHtml5NamedCharacters::VALUE_907[] = {
  0x00b0
};
PRUnichar nsHtml5NamedCharacters::NAME_908[] = {
  'd', 'e', 'g', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_908[] = {
  0x00b0
};
PRUnichar nsHtml5NamedCharacters::NAME_909[] = {
  'd', 'e', 'l', 't', 'a', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_909[] = {
  0x03b4
};
PRUnichar nsHtml5NamedCharacters::NAME_910[] = {
  'd', 'e', 'm', 'p', 't', 'y', 'v', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_910[] = {
  0x29b1
};
PRUnichar nsHtml5NamedCharacters::NAME_911[] = {
  'd', 'f', 'i', 's', 'h', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_911[] = {
  0x297f
};
PRUnichar nsHtml5NamedCharacters::NAME_912[] = {
  'd', 'f', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_912[] = {
  0xd835, 0xdd21
};
PRUnichar nsHtml5NamedCharacters::NAME_913[] = {
  'd', 'h', 'a', 'r', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_913[] = {
  0x21c3
};
PRUnichar nsHtml5NamedCharacters::NAME_914[] = {
  'd', 'h', 'a', 'r', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_914[] = {
  0x21c2
};
PRUnichar nsHtml5NamedCharacters::NAME_915[] = {
  'd', 'i', 'a', 'm', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_915[] = {
  0x22c4
};
PRUnichar nsHtml5NamedCharacters::NAME_916[] = {
  'd', 'i', 'a', 'm', 'o', 'n', 'd', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_916[] = {
  0x22c4
};
PRUnichar nsHtml5NamedCharacters::NAME_917[] = {
  'd', 'i', 'a', 'm', 'o', 'n', 'd', 's', 'u', 'i', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_917[] = {
  0x2666
};
PRUnichar nsHtml5NamedCharacters::NAME_918[] = {
  'd', 'i', 'a', 'm', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_918[] = {
  0x2666
};
PRUnichar nsHtml5NamedCharacters::NAME_919[] = {
  'd', 'i', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_919[] = {
  0x00a8
};
PRUnichar nsHtml5NamedCharacters::NAME_920[] = {
  'd', 'i', 'g', 'a', 'm', 'm', 'a', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_920[] = {
  0x03dd
};
PRUnichar nsHtml5NamedCharacters::NAME_921[] = {
  'd', 'i', 's', 'i', 'n', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_921[] = {
  0x22f2
};
PRUnichar nsHtml5NamedCharacters::NAME_922[] = {
  'd', 'i', 'v', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_922[] = {
  0x00f7
};
PRUnichar nsHtml5NamedCharacters::NAME_923[] = {
  'd', 'i', 'v', 'i', 'd', 'e'
};
PRUnichar nsHtml5NamedCharacters::VALUE_923[] = {
  0x00f7
};
PRUnichar nsHtml5NamedCharacters::NAME_924[] = {
  'd', 'i', 'v', 'i', 'd', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_924[] = {
  0x00f7
};
PRUnichar nsHtml5NamedCharacters::NAME_925[] = {
  'd', 'i', 'v', 'i', 'd', 'e', 'o', 'n', 't', 'i', 'm', 'e', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_925[] = {
  0x22c7
};
PRUnichar nsHtml5NamedCharacters::NAME_926[] = {
  'd', 'i', 'v', 'o', 'n', 'x', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_926[] = {
  0x22c7
};
PRUnichar nsHtml5NamedCharacters::NAME_927[] = {
  'd', 'j', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_927[] = {
  0x0452
};
PRUnichar nsHtml5NamedCharacters::NAME_928[] = {
  'd', 'l', 'c', 'o', 'r', 'n', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_928[] = {
  0x231e
};
PRUnichar nsHtml5NamedCharacters::NAME_929[] = {
  'd', 'l', 'c', 'r', 'o', 'p', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_929[] = {
  0x230d
};
PRUnichar nsHtml5NamedCharacters::NAME_930[] = {
  'd', 'o', 'l', 'l', 'a', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_930[] = {
  0x0024
};
PRUnichar nsHtml5NamedCharacters::NAME_931[] = {
  'd', 'o', 'p', 'f', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_931[] = {
  0xd835, 0xdd55
};
PRUnichar nsHtml5NamedCharacters::NAME_932[] = {
  'd', 'o', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_932[] = {
  0x02d9
};
PRUnichar nsHtml5NamedCharacters::NAME_933[] = {
  'd', 'o', 't', 'e', 'q', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_933[] = {
  0x2250
};
PRUnichar nsHtml5NamedCharacters::NAME_934[] = {
  'd', 'o', 't', 'e', 'q', 'd', 'o', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_934[] = {
  0x2251
};
PRUnichar nsHtml5NamedCharacters::NAME_935[] = {
  'd', 'o', 't', 'm', 'i', 'n', 'u', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_935[] = {
  0x2238
};
PRUnichar nsHtml5NamedCharacters::NAME_936[] = {
  'd', 'o', 't', 'p', 'l', 'u', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_936[] = {
  0x2214
};
PRUnichar nsHtml5NamedCharacters::NAME_937[] = {
  'd', 'o', 't', 's', 'q', 'u', 'a', 'r', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_937[] = {
  0x22a1
};
PRUnichar nsHtml5NamedCharacters::NAME_938[] = {
  'd', 'o', 'u', 'b', 'l', 'e', 'b', 'a', 'r', 'w', 'e', 'd', 'g', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_938[] = {
  0x2306
};
PRUnichar nsHtml5NamedCharacters::NAME_939[] = {
  'd', 'o', 'w', 'n', 'a', 'r', 'r', 'o', 'w', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_939[] = {
  0x2193
};
PRUnichar nsHtml5NamedCharacters::NAME_940[] = {
  'd', 'o', 'w', 'n', 'd', 'o', 'w', 'n', 'a', 'r', 'r', 'o', 'w', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_940[] = {
  0x21ca
};
PRUnichar nsHtml5NamedCharacters::NAME_941[] = {
  'd', 'o', 'w', 'n', 'h', 'a', 'r', 'p', 'o', 'o', 'n', 'l', 'e', 'f', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_941[] = {
  0x21c3
};
PRUnichar nsHtml5NamedCharacters::NAME_942[] = {
  'd', 'o', 'w', 'n', 'h', 'a', 'r', 'p', 'o', 'o', 'n', 'r', 'i', 'g', 'h', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_942[] = {
  0x21c2
};
PRUnichar nsHtml5NamedCharacters::NAME_943[] = {
  'd', 'r', 'b', 'k', 'a', 'r', 'o', 'w', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_943[] = {
  0x2910
};
PRUnichar nsHtml5NamedCharacters::NAME_944[] = {
  'd', 'r', 'c', 'o', 'r', 'n', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_944[] = {
  0x231f
};
PRUnichar nsHtml5NamedCharacters::NAME_945[] = {
  'd', 'r', 'c', 'r', 'o', 'p', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_945[] = {
  0x230c
};
PRUnichar nsHtml5NamedCharacters::NAME_946[] = {
  'd', 's', 'c', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_946[] = {
  0xd835, 0xdcb9
};
PRUnichar nsHtml5NamedCharacters::NAME_947[] = {
  'd', 's', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_947[] = {
  0x0455
};
PRUnichar nsHtml5NamedCharacters::NAME_948[] = {
  'd', 's', 'o', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_948[] = {
  0x29f6
};
PRUnichar nsHtml5NamedCharacters::NAME_949[] = {
  'd', 's', 't', 'r', 'o', 'k', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_949[] = {
  0x0111
};
PRUnichar nsHtml5NamedCharacters::NAME_950[] = {
  'd', 't', 'd', 'o', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_950[] = {
  0x22f1
};
PRUnichar nsHtml5NamedCharacters::NAME_951[] = {
  'd', 't', 'r', 'i', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_951[] = {
  0x25bf
};
PRUnichar nsHtml5NamedCharacters::NAME_952[] = {
  'd', 't', 'r', 'i', 'f', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_952[] = {
  0x25be
};
PRUnichar nsHtml5NamedCharacters::NAME_953[] = {
  'd', 'u', 'a', 'r', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_953[] = {
  0x21f5
};
PRUnichar nsHtml5NamedCharacters::NAME_954[] = {
  'd', 'u', 'h', 'a', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_954[] = {
  0x296f
};
PRUnichar nsHtml5NamedCharacters::NAME_955[] = {
  'd', 'w', 'a', 'n', 'g', 'l', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_955[] = {
  0x29a6
};
PRUnichar nsHtml5NamedCharacters::NAME_956[] = {
  'd', 'z', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_956[] = {
  0x045f
};
PRUnichar nsHtml5NamedCharacters::NAME_957[] = {
  'd', 'z', 'i', 'g', 'r', 'a', 'r', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_957[] = {
  0x27ff
};
PRUnichar nsHtml5NamedCharacters::NAME_958[] = {
  'e', 'D', 'D', 'o', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_958[] = {
  0x2a77
};
PRUnichar nsHtml5NamedCharacters::NAME_959[] = {
  'e', 'D', 'o', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_959[] = {
  0x2251
};
PRUnichar nsHtml5NamedCharacters::NAME_960[] = {
  'e', 'a', 'c', 'u', 't', 'e'
};
PRUnichar nsHtml5NamedCharacters::VALUE_960[] = {
  0x00e9
};
PRUnichar nsHtml5NamedCharacters::NAME_961[] = {
  'e', 'a', 'c', 'u', 't', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_961[] = {
  0x00e9
};
PRUnichar nsHtml5NamedCharacters::NAME_962[] = {
  'e', 'a', 's', 't', 'e', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_962[] = {
  0x2a6e
};
PRUnichar nsHtml5NamedCharacters::NAME_963[] = {
  'e', 'c', 'a', 'r', 'o', 'n', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_963[] = {
  0x011b
};
PRUnichar nsHtml5NamedCharacters::NAME_964[] = {
  'e', 'c', 'i', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_964[] = {
  0x2256
};
PRUnichar nsHtml5NamedCharacters::NAME_965[] = {
  'e', 'c', 'i', 'r', 'c'
};
PRUnichar nsHtml5NamedCharacters::VALUE_965[] = {
  0x00ea
};
PRUnichar nsHtml5NamedCharacters::NAME_966[] = {
  'e', 'c', 'i', 'r', 'c', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_966[] = {
  0x00ea
};
PRUnichar nsHtml5NamedCharacters::NAME_967[] = {
  'e', 'c', 'o', 'l', 'o', 'n', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_967[] = {
  0x2255
};
PRUnichar nsHtml5NamedCharacters::NAME_968[] = {
  'e', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_968[] = {
  0x044d
};
PRUnichar nsHtml5NamedCharacters::NAME_969[] = {
  'e', 'd', 'o', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_969[] = {
  0x0117
};
PRUnichar nsHtml5NamedCharacters::NAME_970[] = {
  'e', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_970[] = {
  0x2147
};
PRUnichar nsHtml5NamedCharacters::NAME_971[] = {
  'e', 'f', 'D', 'o', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_971[] = {
  0x2252
};
PRUnichar nsHtml5NamedCharacters::NAME_972[] = {
  'e', 'f', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_972[] = {
  0xd835, 0xdd22
};
PRUnichar nsHtml5NamedCharacters::NAME_973[] = {
  'e', 'g', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_973[] = {
  0x2a9a
};
PRUnichar nsHtml5NamedCharacters::NAME_974[] = {
  'e', 'g', 'r', 'a', 'v', 'e'
};
PRUnichar nsHtml5NamedCharacters::VALUE_974[] = {
  0x00e8
};
PRUnichar nsHtml5NamedCharacters::NAME_975[] = {
  'e', 'g', 'r', 'a', 'v', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_975[] = {
  0x00e8
};
PRUnichar nsHtml5NamedCharacters::NAME_976[] = {
  'e', 'g', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_976[] = {
  0x2a96
};
PRUnichar nsHtml5NamedCharacters::NAME_977[] = {
  'e', 'g', 's', 'd', 'o', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_977[] = {
  0x2a98
};
PRUnichar nsHtml5NamedCharacters::NAME_978[] = {
  'e', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_978[] = {
  0x2a99
};
PRUnichar nsHtml5NamedCharacters::NAME_979[] = {
  'e', 'l', 'i', 'n', 't', 'e', 'r', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_979[] = {
  0x23e7
};
PRUnichar nsHtml5NamedCharacters::NAME_980[] = {
  'e', 'l', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_980[] = {
  0x2113
};
PRUnichar nsHtml5NamedCharacters::NAME_981[] = {
  'e', 'l', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_981[] = {
  0x2a95
};
PRUnichar nsHtml5NamedCharacters::NAME_982[] = {
  'e', 'l', 's', 'd', 'o', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_982[] = {
  0x2a97
};
PRUnichar nsHtml5NamedCharacters::NAME_983[] = {
  'e', 'm', 'a', 'c', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_983[] = {
  0x0113
};
PRUnichar nsHtml5NamedCharacters::NAME_984[] = {
  'e', 'm', 'p', 't', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_984[] = {
  0x2205
};
PRUnichar nsHtml5NamedCharacters::NAME_985[] = {
  'e', 'm', 'p', 't', 'y', 's', 'e', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_985[] = {
  0x2205
};
PRUnichar nsHtml5NamedCharacters::NAME_986[] = {
  'e', 'm', 'p', 't', 'y', 'v', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_986[] = {
  0x2205
};
PRUnichar nsHtml5NamedCharacters::NAME_987[] = {
  'e', 'm', 's', 'p', '1', '3', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_987[] = {
  0x2004
};
PRUnichar nsHtml5NamedCharacters::NAME_988[] = {
  'e', 'm', 's', 'p', '1', '4', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_988[] = {
  0x2005
};
PRUnichar nsHtml5NamedCharacters::NAME_989[] = {
  'e', 'm', 's', 'p', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_989[] = {
  0x2003
};
PRUnichar nsHtml5NamedCharacters::NAME_990[] = {
  'e', 'n', 'g', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_990[] = {
  0x014b
};
PRUnichar nsHtml5NamedCharacters::NAME_991[] = {
  'e', 'n', 's', 'p', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_991[] = {
  0x2002
};
PRUnichar nsHtml5NamedCharacters::NAME_992[] = {
  'e', 'o', 'g', 'o', 'n', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_992[] = {
  0x0119
};
PRUnichar nsHtml5NamedCharacters::NAME_993[] = {
  'e', 'o', 'p', 'f', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_993[] = {
  0xd835, 0xdd56
};
PRUnichar nsHtml5NamedCharacters::NAME_994[] = {
  'e', 'p', 'a', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_994[] = {
  0x22d5
};
PRUnichar nsHtml5NamedCharacters::NAME_995[] = {
  'e', 'p', 'a', 'r', 's', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_995[] = {
  0x29e3
};
PRUnichar nsHtml5NamedCharacters::NAME_996[] = {
  'e', 'p', 'l', 'u', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_996[] = {
  0x2a71
};
PRUnichar nsHtml5NamedCharacters::NAME_997[] = {
  'e', 'p', 's', 'i', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_997[] = {
  0x03f5
};
PRUnichar nsHtml5NamedCharacters::NAME_998[] = {
  'e', 'p', 's', 'i', 'l', 'o', 'n', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_998[] = {
  0x03b5
};
PRUnichar nsHtml5NamedCharacters::NAME_999[] = {
  'e', 'p', 's', 'i', 'v', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_999[] = {
  0x03b5
};
PRUnichar nsHtml5NamedCharacters::NAME_1000[] = {
  'e', 'q', 'c', 'i', 'r', 'c', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1000[] = {
  0x2256
};
PRUnichar nsHtml5NamedCharacters::NAME_1001[] = {
  'e', 'q', 'c', 'o', 'l', 'o', 'n', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1001[] = {
  0x2255
};
PRUnichar nsHtml5NamedCharacters::NAME_1002[] = {
  'e', 'q', 's', 'i', 'm', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1002[] = {
  0x2242
};
PRUnichar nsHtml5NamedCharacters::NAME_1003[] = {
  'e', 'q', 's', 'l', 'a', 'n', 't', 'g', 't', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1003[] = {
  0x2a96
};
PRUnichar nsHtml5NamedCharacters::NAME_1004[] = {
  'e', 'q', 's', 'l', 'a', 'n', 't', 'l', 'e', 's', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1004[] = {
  0x2a95
};
PRUnichar nsHtml5NamedCharacters::NAME_1005[] = {
  'e', 'q', 'u', 'a', 'l', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1005[] = {
  0x003d
};
PRUnichar nsHtml5NamedCharacters::NAME_1006[] = {
  'e', 'q', 'u', 'e', 's', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1006[] = {
  0x225f
};
PRUnichar nsHtml5NamedCharacters::NAME_1007[] = {
  'e', 'q', 'u', 'i', 'v', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1007[] = {
  0x2261
};
PRUnichar nsHtml5NamedCharacters::NAME_1008[] = {
  'e', 'q', 'u', 'i', 'v', 'D', 'D', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1008[] = {
  0x2a78
};
PRUnichar nsHtml5NamedCharacters::NAME_1009[] = {
  'e', 'q', 'v', 'p', 'a', 'r', 's', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1009[] = {
  0x29e5
};
PRUnichar nsHtml5NamedCharacters::NAME_1010[] = {
  'e', 'r', 'D', 'o', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1010[] = {
  0x2253
};
PRUnichar nsHtml5NamedCharacters::NAME_1011[] = {
  'e', 'r', 'a', 'r', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1011[] = {
  0x2971
};
PRUnichar nsHtml5NamedCharacters::NAME_1012[] = {
  'e', 's', 'c', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1012[] = {
  0x212f
};
PRUnichar nsHtml5NamedCharacters::NAME_1013[] = {
  'e', 's', 'd', 'o', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1013[] = {
  0x2250
};
PRUnichar nsHtml5NamedCharacters::NAME_1014[] = {
  'e', 's', 'i', 'm', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1014[] = {
  0x2242
};
PRUnichar nsHtml5NamedCharacters::NAME_1015[] = {
  'e', 't', 'a', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1015[] = {
  0x03b7
};
PRUnichar nsHtml5NamedCharacters::NAME_1016[] = {
  'e', 't', 'h'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1016[] = {
  0x00f0
};
PRUnichar nsHtml5NamedCharacters::NAME_1017[] = {
  'e', 't', 'h', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1017[] = {
  0x00f0
};
PRUnichar nsHtml5NamedCharacters::NAME_1018[] = {
  'e', 'u', 'm', 'l'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1018[] = {
  0x00eb
};
PRUnichar nsHtml5NamedCharacters::NAME_1019[] = {
  'e', 'u', 'm', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1019[] = {
  0x00eb
};
PRUnichar nsHtml5NamedCharacters::NAME_1020[] = {
  'e', 'u', 'r', 'o', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1020[] = {
  0x20ac
};
PRUnichar nsHtml5NamedCharacters::NAME_1021[] = {
  'e', 'x', 'c', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1021[] = {
  0x0021
};
PRUnichar nsHtml5NamedCharacters::NAME_1022[] = {
  'e', 'x', 'i', 's', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1022[] = {
  0x2203
};
PRUnichar nsHtml5NamedCharacters::NAME_1023[] = {
  'e', 'x', 'p', 'e', 'c', 't', 'a', 't', 'i', 'o', 'n', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1023[] = {
  0x2130
};
PRUnichar nsHtml5NamedCharacters::NAME_1024[] = {
  'e', 'x', 'p', 'o', 'n', 'e', 'n', 't', 'i', 'a', 'l', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1024[] = {
  0x2147
};
PRUnichar nsHtml5NamedCharacters::NAME_1025[] = {
  'f', 'a', 'l', 'l', 'i', 'n', 'g', 'd', 'o', 't', 's', 'e', 'q', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1025[] = {
  0x2252
};
PRUnichar nsHtml5NamedCharacters::NAME_1026[] = {
  'f', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1026[] = {
  0x0444
};
PRUnichar nsHtml5NamedCharacters::NAME_1027[] = {
  'f', 'e', 'm', 'a', 'l', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1027[] = {
  0x2640
};
PRUnichar nsHtml5NamedCharacters::NAME_1028[] = {
  'f', 'f', 'i', 'l', 'i', 'g', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1028[] = {
  0xfb03
};
PRUnichar nsHtml5NamedCharacters::NAME_1029[] = {
  'f', 'f', 'l', 'i', 'g', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1029[] = {
  0xfb00
};
PRUnichar nsHtml5NamedCharacters::NAME_1030[] = {
  'f', 'f', 'l', 'l', 'i', 'g', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1030[] = {
  0xfb04
};
PRUnichar nsHtml5NamedCharacters::NAME_1031[] = {
  'f', 'f', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1031[] = {
  0xd835, 0xdd23
};
PRUnichar nsHtml5NamedCharacters::NAME_1032[] = {
  'f', 'i', 'l', 'i', 'g', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1032[] = {
  0xfb01
};
PRUnichar nsHtml5NamedCharacters::NAME_1033[] = {
  'f', 'l', 'a', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1033[] = {
  0x266d
};
PRUnichar nsHtml5NamedCharacters::NAME_1034[] = {
  'f', 'l', 'l', 'i', 'g', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1034[] = {
  0xfb02
};
PRUnichar nsHtml5NamedCharacters::NAME_1035[] = {
  'f', 'l', 't', 'n', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1035[] = {
  0x25b1
};
PRUnichar nsHtml5NamedCharacters::NAME_1036[] = {
  'f', 'n', 'o', 'f', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1036[] = {
  0x0192
};
PRUnichar nsHtml5NamedCharacters::NAME_1037[] = {
  'f', 'o', 'p', 'f', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1037[] = {
  0xd835, 0xdd57
};
PRUnichar nsHtml5NamedCharacters::NAME_1038[] = {
  'f', 'o', 'r', 'a', 'l', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1038[] = {
  0x2200
};
PRUnichar nsHtml5NamedCharacters::NAME_1039[] = {
  'f', 'o', 'r', 'k', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1039[] = {
  0x22d4
};
PRUnichar nsHtml5NamedCharacters::NAME_1040[] = {
  'f', 'o', 'r', 'k', 'v', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1040[] = {
  0x2ad9
};
PRUnichar nsHtml5NamedCharacters::NAME_1041[] = {
  'f', 'p', 'a', 'r', 't', 'i', 'n', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1041[] = {
  0x2a0d
};
PRUnichar nsHtml5NamedCharacters::NAME_1042[] = {
  'f', 'r', 'a', 'c', '1', '2'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1042[] = {
  0x00bd
};
PRUnichar nsHtml5NamedCharacters::NAME_1043[] = {
  'f', 'r', 'a', 'c', '1', '2', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1043[] = {
  0x00bd
};
PRUnichar nsHtml5NamedCharacters::NAME_1044[] = {
  'f', 'r', 'a', 'c', '1', '3', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1044[] = {
  0x2153
};
PRUnichar nsHtml5NamedCharacters::NAME_1045[] = {
  'f', 'r', 'a', 'c', '1', '4'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1045[] = {
  0x00bc
};
PRUnichar nsHtml5NamedCharacters::NAME_1046[] = {
  'f', 'r', 'a', 'c', '1', '4', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1046[] = {
  0x00bc
};
PRUnichar nsHtml5NamedCharacters::NAME_1047[] = {
  'f', 'r', 'a', 'c', '1', '5', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1047[] = {
  0x2155
};
PRUnichar nsHtml5NamedCharacters::NAME_1048[] = {
  'f', 'r', 'a', 'c', '1', '6', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1048[] = {
  0x2159
};
PRUnichar nsHtml5NamedCharacters::NAME_1049[] = {
  'f', 'r', 'a', 'c', '1', '8', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1049[] = {
  0x215b
};
PRUnichar nsHtml5NamedCharacters::NAME_1050[] = {
  'f', 'r', 'a', 'c', '2', '3', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1050[] = {
  0x2154
};
PRUnichar nsHtml5NamedCharacters::NAME_1051[] = {
  'f', 'r', 'a', 'c', '2', '5', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1051[] = {
  0x2156
};
PRUnichar nsHtml5NamedCharacters::NAME_1052[] = {
  'f', 'r', 'a', 'c', '3', '4'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1052[] = {
  0x00be
};
PRUnichar nsHtml5NamedCharacters::NAME_1053[] = {
  'f', 'r', 'a', 'c', '3', '4', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1053[] = {
  0x00be
};
PRUnichar nsHtml5NamedCharacters::NAME_1054[] = {
  'f', 'r', 'a', 'c', '3', '5', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1054[] = {
  0x2157
};
PRUnichar nsHtml5NamedCharacters::NAME_1055[] = {
  'f', 'r', 'a', 'c', '3', '8', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1055[] = {
  0x215c
};
PRUnichar nsHtml5NamedCharacters::NAME_1056[] = {
  'f', 'r', 'a', 'c', '4', '5', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1056[] = {
  0x2158
};
PRUnichar nsHtml5NamedCharacters::NAME_1057[] = {
  'f', 'r', 'a', 'c', '5', '6', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1057[] = {
  0x215a
};
PRUnichar nsHtml5NamedCharacters::NAME_1058[] = {
  'f', 'r', 'a', 'c', '5', '8', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1058[] = {
  0x215d
};
PRUnichar nsHtml5NamedCharacters::NAME_1059[] = {
  'f', 'r', 'a', 'c', '7', '8', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1059[] = {
  0x215e
};
PRUnichar nsHtml5NamedCharacters::NAME_1060[] = {
  'f', 'r', 'a', 's', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1060[] = {
  0x2044
};
PRUnichar nsHtml5NamedCharacters::NAME_1061[] = {
  'f', 'r', 'o', 'w', 'n', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1061[] = {
  0x2322
};
PRUnichar nsHtml5NamedCharacters::NAME_1062[] = {
  'f', 's', 'c', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1062[] = {
  0xd835, 0xdcbb
};
PRUnichar nsHtml5NamedCharacters::NAME_1063[] = {
  'g', 'E', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1063[] = {
  0x2267
};
PRUnichar nsHtml5NamedCharacters::NAME_1064[] = {
  'g', 'E', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1064[] = {
  0x2a8c
};
PRUnichar nsHtml5NamedCharacters::NAME_1065[] = {
  'g', 'a', 'c', 'u', 't', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1065[] = {
  0x01f5
};
PRUnichar nsHtml5NamedCharacters::NAME_1066[] = {
  'g', 'a', 'm', 'm', 'a', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1066[] = {
  0x03b3
};
PRUnichar nsHtml5NamedCharacters::NAME_1067[] = {
  'g', 'a', 'm', 'm', 'a', 'd', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1067[] = {
  0x03dd
};
PRUnichar nsHtml5NamedCharacters::NAME_1068[] = {
  'g', 'a', 'p', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1068[] = {
  0x2a86
};
PRUnichar nsHtml5NamedCharacters::NAME_1069[] = {
  'g', 'b', 'r', 'e', 'v', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1069[] = {
  0x011f
};
PRUnichar nsHtml5NamedCharacters::NAME_1070[] = {
  'g', 'c', 'i', 'r', 'c', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1070[] = {
  0x011d
};
PRUnichar nsHtml5NamedCharacters::NAME_1071[] = {
  'g', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1071[] = {
  0x0433
};
PRUnichar nsHtml5NamedCharacters::NAME_1072[] = {
  'g', 'd', 'o', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1072[] = {
  0x0121
};
PRUnichar nsHtml5NamedCharacters::NAME_1073[] = {
  'g', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1073[] = {
  0x2265
};
PRUnichar nsHtml5NamedCharacters::NAME_1074[] = {
  'g', 'e', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1074[] = {
  0x22db
};
PRUnichar nsHtml5NamedCharacters::NAME_1075[] = {
  'g', 'e', 'q', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1075[] = {
  0x2265
};
PRUnichar nsHtml5NamedCharacters::NAME_1076[] = {
  'g', 'e', 'q', 'q', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1076[] = {
  0x2267
};
PRUnichar nsHtml5NamedCharacters::NAME_1077[] = {
  'g', 'e', 'q', 's', 'l', 'a', 'n', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1077[] = {
  0x2a7e
};
PRUnichar nsHtml5NamedCharacters::NAME_1078[] = {
  'g', 'e', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1078[] = {
  0x2a7e
};
PRUnichar nsHtml5NamedCharacters::NAME_1079[] = {
  'g', 'e', 's', 'c', 'c', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1079[] = {
  0x2aa9
};
PRUnichar nsHtml5NamedCharacters::NAME_1080[] = {
  'g', 'e', 's', 'd', 'o', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1080[] = {
  0x2a80
};
PRUnichar nsHtml5NamedCharacters::NAME_1081[] = {
  'g', 'e', 's', 'd', 'o', 't', 'o', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1081[] = {
  0x2a82
};
PRUnichar nsHtml5NamedCharacters::NAME_1082[] = {
  'g', 'e', 's', 'd', 'o', 't', 'o', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1082[] = {
  0x2a84
};
PRUnichar nsHtml5NamedCharacters::NAME_1083[] = {
  'g', 'e', 's', 'l', 'e', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1083[] = {
  0x2a94
};
PRUnichar nsHtml5NamedCharacters::NAME_1084[] = {
  'g', 'f', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1084[] = {
  0xd835, 0xdd24
};
PRUnichar nsHtml5NamedCharacters::NAME_1085[] = {
  'g', 'g', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1085[] = {
  0x226b
};
PRUnichar nsHtml5NamedCharacters::NAME_1086[] = {
  'g', 'g', 'g', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1086[] = {
  0x22d9
};
PRUnichar nsHtml5NamedCharacters::NAME_1087[] = {
  'g', 'i', 'm', 'e', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1087[] = {
  0x2137
};
PRUnichar nsHtml5NamedCharacters::NAME_1088[] = {
  'g', 'j', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1088[] = {
  0x0453
};
PRUnichar nsHtml5NamedCharacters::NAME_1089[] = {
  'g', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1089[] = {
  0x2277
};
PRUnichar nsHtml5NamedCharacters::NAME_1090[] = {
  'g', 'l', 'E', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1090[] = {
  0x2a92
};
PRUnichar nsHtml5NamedCharacters::NAME_1091[] = {
  'g', 'l', 'a', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1091[] = {
  0x2aa5
};
PRUnichar nsHtml5NamedCharacters::NAME_1092[] = {
  'g', 'l', 'j', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1092[] = {
  0x2aa4
};
PRUnichar nsHtml5NamedCharacters::NAME_1093[] = {
  'g', 'n', 'E', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1093[] = {
  0x2269
};
PRUnichar nsHtml5NamedCharacters::NAME_1094[] = {
  'g', 'n', 'a', 'p', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1094[] = {
  0x2a8a
};
PRUnichar nsHtml5NamedCharacters::NAME_1095[] = {
  'g', 'n', 'a', 'p', 'p', 'r', 'o', 'x', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1095[] = {
  0x2a8a
};
PRUnichar nsHtml5NamedCharacters::NAME_1096[] = {
  'g', 'n', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1096[] = {
  0x2a88
};
PRUnichar nsHtml5NamedCharacters::NAME_1097[] = {
  'g', 'n', 'e', 'q', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1097[] = {
  0x2a88
};
PRUnichar nsHtml5NamedCharacters::NAME_1098[] = {
  'g', 'n', 'e', 'q', 'q', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1098[] = {
  0x2269
};
PRUnichar nsHtml5NamedCharacters::NAME_1099[] = {
  'g', 'n', 's', 'i', 'm', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1099[] = {
  0x22e7
};
PRUnichar nsHtml5NamedCharacters::NAME_1100[] = {
  'g', 'o', 'p', 'f', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1100[] = {
  0xd835, 0xdd58
};
PRUnichar nsHtml5NamedCharacters::NAME_1101[] = {
  'g', 'r', 'a', 'v', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1101[] = {
  0x0060
};
PRUnichar nsHtml5NamedCharacters::NAME_1102[] = {
  'g', 's', 'c', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1102[] = {
  0x210a
};
PRUnichar nsHtml5NamedCharacters::NAME_1103[] = {
  'g', 's', 'i', 'm', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1103[] = {
  0x2273
};
PRUnichar nsHtml5NamedCharacters::NAME_1104[] = {
  'g', 's', 'i', 'm', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1104[] = {
  0x2a8e
};
PRUnichar nsHtml5NamedCharacters::NAME_1105[] = {
  'g', 's', 'i', 'm', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1105[] = {
  0x2a90
};
PRUnichar nsHtml5NamedCharacters::NAME_1106[] = {
  'g', 't'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1106[] = {
  0x003e
};
PRUnichar nsHtml5NamedCharacters::NAME_1107[] = {
  'g', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1107[] = {
  0x003e
};
PRUnichar nsHtml5NamedCharacters::NAME_1108[] = {
  'g', 't', 'c', 'c', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1108[] = {
  0x2aa7
};
PRUnichar nsHtml5NamedCharacters::NAME_1109[] = {
  'g', 't', 'c', 'i', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1109[] = {
  0x2a7a
};
PRUnichar nsHtml5NamedCharacters::NAME_1110[] = {
  'g', 't', 'd', 'o', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1110[] = {
  0x22d7
};
PRUnichar nsHtml5NamedCharacters::NAME_1111[] = {
  'g', 't', 'l', 'P', 'a', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1111[] = {
  0x2995
};
PRUnichar nsHtml5NamedCharacters::NAME_1112[] = {
  'g', 't', 'q', 'u', 'e', 's', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1112[] = {
  0x2a7c
};
PRUnichar nsHtml5NamedCharacters::NAME_1113[] = {
  'g', 't', 'r', 'a', 'p', 'p', 'r', 'o', 'x', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1113[] = {
  0x2a86
};
PRUnichar nsHtml5NamedCharacters::NAME_1114[] = {
  'g', 't', 'r', 'a', 'r', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1114[] = {
  0x2978
};
PRUnichar nsHtml5NamedCharacters::NAME_1115[] = {
  'g', 't', 'r', 'd', 'o', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1115[] = {
  0x22d7
};
PRUnichar nsHtml5NamedCharacters::NAME_1116[] = {
  'g', 't', 'r', 'e', 'q', 'l', 'e', 's', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1116[] = {
  0x22db
};
PRUnichar nsHtml5NamedCharacters::NAME_1117[] = {
  'g', 't', 'r', 'e', 'q', 'q', 'l', 'e', 's', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1117[] = {
  0x2a8c
};
PRUnichar nsHtml5NamedCharacters::NAME_1118[] = {
  'g', 't', 'r', 'l', 'e', 's', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1118[] = {
  0x2277
};
PRUnichar nsHtml5NamedCharacters::NAME_1119[] = {
  'g', 't', 'r', 's', 'i', 'm', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1119[] = {
  0x2273
};
PRUnichar nsHtml5NamedCharacters::NAME_1120[] = {
  'h', 'A', 'r', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1120[] = {
  0x21d4
};
PRUnichar nsHtml5NamedCharacters::NAME_1121[] = {
  'h', 'a', 'i', 'r', 's', 'p', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1121[] = {
  0x200a
};
PRUnichar nsHtml5NamedCharacters::NAME_1122[] = {
  'h', 'a', 'l', 'f', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1122[] = {
  0x00bd
};
PRUnichar nsHtml5NamedCharacters::NAME_1123[] = {
  'h', 'a', 'm', 'i', 'l', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1123[] = {
  0x210b
};
PRUnichar nsHtml5NamedCharacters::NAME_1124[] = {
  'h', 'a', 'r', 'd', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1124[] = {
  0x044a
};
PRUnichar nsHtml5NamedCharacters::NAME_1125[] = {
  'h', 'a', 'r', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1125[] = {
  0x2194
};
PRUnichar nsHtml5NamedCharacters::NAME_1126[] = {
  'h', 'a', 'r', 'r', 'c', 'i', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1126[] = {
  0x2948
};
PRUnichar nsHtml5NamedCharacters::NAME_1127[] = {
  'h', 'a', 'r', 'r', 'w', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1127[] = {
  0x21ad
};
PRUnichar nsHtml5NamedCharacters::NAME_1128[] = {
  'h', 'b', 'a', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1128[] = {
  0x210f
};
PRUnichar nsHtml5NamedCharacters::NAME_1129[] = {
  'h', 'c', 'i', 'r', 'c', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1129[] = {
  0x0125
};
PRUnichar nsHtml5NamedCharacters::NAME_1130[] = {
  'h', 'e', 'a', 'r', 't', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1130[] = {
  0x2665
};
PRUnichar nsHtml5NamedCharacters::NAME_1131[] = {
  'h', 'e', 'a', 'r', 't', 's', 'u', 'i', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1131[] = {
  0x2665
};
PRUnichar nsHtml5NamedCharacters::NAME_1132[] = {
  'h', 'e', 'l', 'l', 'i', 'p', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1132[] = {
  0x2026
};
PRUnichar nsHtml5NamedCharacters::NAME_1133[] = {
  'h', 'e', 'r', 'c', 'o', 'n', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1133[] = {
  0x22b9
};
PRUnichar nsHtml5NamedCharacters::NAME_1134[] = {
  'h', 'f', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1134[] = {
  0xd835, 0xdd25
};
PRUnichar nsHtml5NamedCharacters::NAME_1135[] = {
  'h', 'k', 's', 'e', 'a', 'r', 'o', 'w', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1135[] = {
  0x2925
};
PRUnichar nsHtml5NamedCharacters::NAME_1136[] = {
  'h', 'k', 's', 'w', 'a', 'r', 'o', 'w', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1136[] = {
  0x2926
};
PRUnichar nsHtml5NamedCharacters::NAME_1137[] = {
  'h', 'o', 'a', 'r', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1137[] = {
  0x21ff
};
PRUnichar nsHtml5NamedCharacters::NAME_1138[] = {
  'h', 'o', 'm', 't', 'h', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1138[] = {
  0x223b
};
PRUnichar nsHtml5NamedCharacters::NAME_1139[] = {
  'h', 'o', 'o', 'k', 'l', 'e', 'f', 't', 'a', 'r', 'r', 'o', 'w', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1139[] = {
  0x21a9
};
PRUnichar nsHtml5NamedCharacters::NAME_1140[] = {
  'h', 'o', 'o', 'k', 'r', 'i', 'g', 'h', 't', 'a', 'r', 'r', 'o', 'w', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1140[] = {
  0x21aa
};
PRUnichar nsHtml5NamedCharacters::NAME_1141[] = {
  'h', 'o', 'p', 'f', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1141[] = {
  0xd835, 0xdd59
};
PRUnichar nsHtml5NamedCharacters::NAME_1142[] = {
  'h', 'o', 'r', 'b', 'a', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1142[] = {
  0x2015
};
PRUnichar nsHtml5NamedCharacters::NAME_1143[] = {
  'h', 's', 'c', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1143[] = {
  0xd835, 0xdcbd
};
PRUnichar nsHtml5NamedCharacters::NAME_1144[] = {
  'h', 's', 'l', 'a', 's', 'h', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1144[] = {
  0x210f
};
PRUnichar nsHtml5NamedCharacters::NAME_1145[] = {
  'h', 's', 't', 'r', 'o', 'k', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1145[] = {
  0x0127
};
PRUnichar nsHtml5NamedCharacters::NAME_1146[] = {
  'h', 'y', 'b', 'u', 'l', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1146[] = {
  0x2043
};
PRUnichar nsHtml5NamedCharacters::NAME_1147[] = {
  'h', 'y', 'p', 'h', 'e', 'n', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1147[] = {
  0x2010
};
PRUnichar nsHtml5NamedCharacters::NAME_1148[] = {
  'i', 'a', 'c', 'u', 't', 'e'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1148[] = {
  0x00ed
};
PRUnichar nsHtml5NamedCharacters::NAME_1149[] = {
  'i', 'a', 'c', 'u', 't', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1149[] = {
  0x00ed
};
PRUnichar nsHtml5NamedCharacters::NAME_1150[] = {
  'i', 'c', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1150[] = {
  0x2063
};
PRUnichar nsHtml5NamedCharacters::NAME_1151[] = {
  'i', 'c', 'i', 'r', 'c'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1151[] = {
  0x00ee
};
PRUnichar nsHtml5NamedCharacters::NAME_1152[] = {
  'i', 'c', 'i', 'r', 'c', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1152[] = {
  0x00ee
};
PRUnichar nsHtml5NamedCharacters::NAME_1153[] = {
  'i', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1153[] = {
  0x0438
};
PRUnichar nsHtml5NamedCharacters::NAME_1154[] = {
  'i', 'e', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1154[] = {
  0x0435
};
PRUnichar nsHtml5NamedCharacters::NAME_1155[] = {
  'i', 'e', 'x', 'c', 'l'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1155[] = {
  0x00a1
};
PRUnichar nsHtml5NamedCharacters::NAME_1156[] = {
  'i', 'e', 'x', 'c', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1156[] = {
  0x00a1
};
PRUnichar nsHtml5NamedCharacters::NAME_1157[] = {
  'i', 'f', 'f', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1157[] = {
  0x21d4
};
PRUnichar nsHtml5NamedCharacters::NAME_1158[] = {
  'i', 'f', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1158[] = {
  0xd835, 0xdd26
};
PRUnichar nsHtml5NamedCharacters::NAME_1159[] = {
  'i', 'g', 'r', 'a', 'v', 'e'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1159[] = {
  0x00ec
};
PRUnichar nsHtml5NamedCharacters::NAME_1160[] = {
  'i', 'g', 'r', 'a', 'v', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1160[] = {
  0x00ec
};
PRUnichar nsHtml5NamedCharacters::NAME_1161[] = {
  'i', 'i', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1161[] = {
  0x2148
};
PRUnichar nsHtml5NamedCharacters::NAME_1162[] = {
  'i', 'i', 'i', 'i', 'n', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1162[] = {
  0x2a0c
};
PRUnichar nsHtml5NamedCharacters::NAME_1163[] = {
  'i', 'i', 'i', 'n', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1163[] = {
  0x222d
};
PRUnichar nsHtml5NamedCharacters::NAME_1164[] = {
  'i', 'i', 'n', 'f', 'i', 'n', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1164[] = {
  0x29dc
};
PRUnichar nsHtml5NamedCharacters::NAME_1165[] = {
  'i', 'i', 'o', 't', 'a', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1165[] = {
  0x2129
};
PRUnichar nsHtml5NamedCharacters::NAME_1166[] = {
  'i', 'j', 'l', 'i', 'g', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1166[] = {
  0x0133
};
PRUnichar nsHtml5NamedCharacters::NAME_1167[] = {
  'i', 'm', 'a', 'c', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1167[] = {
  0x012b
};
PRUnichar nsHtml5NamedCharacters::NAME_1168[] = {
  'i', 'm', 'a', 'g', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1168[] = {
  0x2111
};
PRUnichar nsHtml5NamedCharacters::NAME_1169[] = {
  'i', 'm', 'a', 'g', 'l', 'i', 'n', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1169[] = {
  0x2110
};
PRUnichar nsHtml5NamedCharacters::NAME_1170[] = {
  'i', 'm', 'a', 'g', 'p', 'a', 'r', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1170[] = {
  0x2111
};
PRUnichar nsHtml5NamedCharacters::NAME_1171[] = {
  'i', 'm', 'a', 't', 'h', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1171[] = {
  0x0131
};
PRUnichar nsHtml5NamedCharacters::NAME_1172[] = {
  'i', 'm', 'o', 'f', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1172[] = {
  0x22b7
};
PRUnichar nsHtml5NamedCharacters::NAME_1173[] = {
  'i', 'm', 'p', 'e', 'd', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1173[] = {
  0x01b5
};
PRUnichar nsHtml5NamedCharacters::NAME_1174[] = {
  'i', 'n', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1174[] = {
  0x2208
};
PRUnichar nsHtml5NamedCharacters::NAME_1175[] = {
  'i', 'n', 'c', 'a', 'r', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1175[] = {
  0x2105
};
PRUnichar nsHtml5NamedCharacters::NAME_1176[] = {
  'i', 'n', 'f', 'i', 'n', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1176[] = {
  0x221e
};
PRUnichar nsHtml5NamedCharacters::NAME_1177[] = {
  'i', 'n', 'f', 'i', 'n', 't', 'i', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1177[] = {
  0x29dd
};
PRUnichar nsHtml5NamedCharacters::NAME_1178[] = {
  'i', 'n', 'o', 'd', 'o', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1178[] = {
  0x0131
};
PRUnichar nsHtml5NamedCharacters::NAME_1179[] = {
  'i', 'n', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1179[] = {
  0x222b
};
PRUnichar nsHtml5NamedCharacters::NAME_1180[] = {
  'i', 'n', 't', 'c', 'a', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1180[] = {
  0x22ba
};
PRUnichar nsHtml5NamedCharacters::NAME_1181[] = {
  'i', 'n', 't', 'e', 'g', 'e', 'r', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1181[] = {
  0x2124
};
PRUnichar nsHtml5NamedCharacters::NAME_1182[] = {
  'i', 'n', 't', 'e', 'r', 'c', 'a', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1182[] = {
  0x22ba
};
PRUnichar nsHtml5NamedCharacters::NAME_1183[] = {
  'i', 'n', 't', 'l', 'a', 'r', 'h', 'k', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1183[] = {
  0x2a17
};
PRUnichar nsHtml5NamedCharacters::NAME_1184[] = {
  'i', 'n', 't', 'p', 'r', 'o', 'd', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1184[] = {
  0x2a3c
};
PRUnichar nsHtml5NamedCharacters::NAME_1185[] = {
  'i', 'o', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1185[] = {
  0x0451
};
PRUnichar nsHtml5NamedCharacters::NAME_1186[] = {
  'i', 'o', 'g', 'o', 'n', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1186[] = {
  0x012f
};
PRUnichar nsHtml5NamedCharacters::NAME_1187[] = {
  'i', 'o', 'p', 'f', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1187[] = {
  0xd835, 0xdd5a
};
PRUnichar nsHtml5NamedCharacters::NAME_1188[] = {
  'i', 'o', 't', 'a', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1188[] = {
  0x03b9
};
PRUnichar nsHtml5NamedCharacters::NAME_1189[] = {
  'i', 'p', 'r', 'o', 'd', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1189[] = {
  0x2a3c
};
PRUnichar nsHtml5NamedCharacters::NAME_1190[] = {
  'i', 'q', 'u', 'e', 's', 't'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1190[] = {
  0x00bf
};
PRUnichar nsHtml5NamedCharacters::NAME_1191[] = {
  'i', 'q', 'u', 'e', 's', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1191[] = {
  0x00bf
};
PRUnichar nsHtml5NamedCharacters::NAME_1192[] = {
  'i', 's', 'c', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1192[] = {
  0xd835, 0xdcbe
};
PRUnichar nsHtml5NamedCharacters::NAME_1193[] = {
  'i', 's', 'i', 'n', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1193[] = {
  0x2208
};
PRUnichar nsHtml5NamedCharacters::NAME_1194[] = {
  'i', 's', 'i', 'n', 'E', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1194[] = {
  0x22f9
};
PRUnichar nsHtml5NamedCharacters::NAME_1195[] = {
  'i', 's', 'i', 'n', 'd', 'o', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1195[] = {
  0x22f5
};
PRUnichar nsHtml5NamedCharacters::NAME_1196[] = {
  'i', 's', 'i', 'n', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1196[] = {
  0x22f4
};
PRUnichar nsHtml5NamedCharacters::NAME_1197[] = {
  'i', 's', 'i', 'n', 's', 'v', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1197[] = {
  0x22f3
};
PRUnichar nsHtml5NamedCharacters::NAME_1198[] = {
  'i', 's', 'i', 'n', 'v', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1198[] = {
  0x2208
};
PRUnichar nsHtml5NamedCharacters::NAME_1199[] = {
  'i', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1199[] = {
  0x2062
};
PRUnichar nsHtml5NamedCharacters::NAME_1200[] = {
  'i', 't', 'i', 'l', 'd', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1200[] = {
  0x0129
};
PRUnichar nsHtml5NamedCharacters::NAME_1201[] = {
  'i', 'u', 'k', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1201[] = {
  0x0456
};
PRUnichar nsHtml5NamedCharacters::NAME_1202[] = {
  'i', 'u', 'm', 'l'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1202[] = {
  0x00ef
};
PRUnichar nsHtml5NamedCharacters::NAME_1203[] = {
  'i', 'u', 'm', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1203[] = {
  0x00ef
};
PRUnichar nsHtml5NamedCharacters::NAME_1204[] = {
  'j', 'c', 'i', 'r', 'c', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1204[] = {
  0x0135
};
PRUnichar nsHtml5NamedCharacters::NAME_1205[] = {
  'j', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1205[] = {
  0x0439
};
PRUnichar nsHtml5NamedCharacters::NAME_1206[] = {
  'j', 'f', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1206[] = {
  0xd835, 0xdd27
};
PRUnichar nsHtml5NamedCharacters::NAME_1207[] = {
  'j', 'm', 'a', 't', 'h', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1207[] = {
  0x0237
};
PRUnichar nsHtml5NamedCharacters::NAME_1208[] = {
  'j', 'o', 'p', 'f', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1208[] = {
  0xd835, 0xdd5b
};
PRUnichar nsHtml5NamedCharacters::NAME_1209[] = {
  'j', 's', 'c', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1209[] = {
  0xd835, 0xdcbf
};
PRUnichar nsHtml5NamedCharacters::NAME_1210[] = {
  'j', 's', 'e', 'r', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1210[] = {
  0x0458
};
PRUnichar nsHtml5NamedCharacters::NAME_1211[] = {
  'j', 'u', 'k', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1211[] = {
  0x0454
};
PRUnichar nsHtml5NamedCharacters::NAME_1212[] = {
  'k', 'a', 'p', 'p', 'a', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1212[] = {
  0x03ba
};
PRUnichar nsHtml5NamedCharacters::NAME_1213[] = {
  'k', 'a', 'p', 'p', 'a', 'v', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1213[] = {
  0x03f0
};
PRUnichar nsHtml5NamedCharacters::NAME_1214[] = {
  'k', 'c', 'e', 'd', 'i', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1214[] = {
  0x0137
};
PRUnichar nsHtml5NamedCharacters::NAME_1215[] = {
  'k', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1215[] = {
  0x043a
};
PRUnichar nsHtml5NamedCharacters::NAME_1216[] = {
  'k', 'f', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1216[] = {
  0xd835, 0xdd28
};
PRUnichar nsHtml5NamedCharacters::NAME_1217[] = {
  'k', 'g', 'r', 'e', 'e', 'n', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1217[] = {
  0x0138
};
PRUnichar nsHtml5NamedCharacters::NAME_1218[] = {
  'k', 'h', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1218[] = {
  0x0445
};
PRUnichar nsHtml5NamedCharacters::NAME_1219[] = {
  'k', 'j', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1219[] = {
  0x045c
};
PRUnichar nsHtml5NamedCharacters::NAME_1220[] = {
  'k', 'o', 'p', 'f', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1220[] = {
  0xd835, 0xdd5c
};
PRUnichar nsHtml5NamedCharacters::NAME_1221[] = {
  'k', 's', 'c', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1221[] = {
  0xd835, 0xdcc0
};
PRUnichar nsHtml5NamedCharacters::NAME_1222[] = {
  'l', 'A', 'a', 'r', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1222[] = {
  0x21da
};
PRUnichar nsHtml5NamedCharacters::NAME_1223[] = {
  'l', 'A', 'r', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1223[] = {
  0x21d0
};
PRUnichar nsHtml5NamedCharacters::NAME_1224[] = {
  'l', 'A', 't', 'a', 'i', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1224[] = {
  0x291b
};
PRUnichar nsHtml5NamedCharacters::NAME_1225[] = {
  'l', 'B', 'a', 'r', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1225[] = {
  0x290e
};
PRUnichar nsHtml5NamedCharacters::NAME_1226[] = {
  'l', 'E', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1226[] = {
  0x2266
};
PRUnichar nsHtml5NamedCharacters::NAME_1227[] = {
  'l', 'E', 'g', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1227[] = {
  0x2a8b
};
PRUnichar nsHtml5NamedCharacters::NAME_1228[] = {
  'l', 'H', 'a', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1228[] = {
  0x2962
};
PRUnichar nsHtml5NamedCharacters::NAME_1229[] = {
  'l', 'a', 'c', 'u', 't', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1229[] = {
  0x013a
};
PRUnichar nsHtml5NamedCharacters::NAME_1230[] = {
  'l', 'a', 'e', 'm', 'p', 't', 'y', 'v', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1230[] = {
  0x29b4
};
PRUnichar nsHtml5NamedCharacters::NAME_1231[] = {
  'l', 'a', 'g', 'r', 'a', 'n', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1231[] = {
  0x2112
};
PRUnichar nsHtml5NamedCharacters::NAME_1232[] = {
  'l', 'a', 'm', 'b', 'd', 'a', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1232[] = {
  0x03bb
};
PRUnichar nsHtml5NamedCharacters::NAME_1233[] = {
  'l', 'a', 'n', 'g', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1233[] = {
  0x27e8
};
PRUnichar nsHtml5NamedCharacters::NAME_1234[] = {
  'l', 'a', 'n', 'g', 'd', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1234[] = {
  0x2991
};
PRUnichar nsHtml5NamedCharacters::NAME_1235[] = {
  'l', 'a', 'n', 'g', 'l', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1235[] = {
  0x27e8
};
PRUnichar nsHtml5NamedCharacters::NAME_1236[] = {
  'l', 'a', 'p', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1236[] = {
  0x2a85
};
PRUnichar nsHtml5NamedCharacters::NAME_1237[] = {
  'l', 'a', 'q', 'u', 'o'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1237[] = {
  0x00ab
};
PRUnichar nsHtml5NamedCharacters::NAME_1238[] = {
  'l', 'a', 'q', 'u', 'o', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1238[] = {
  0x00ab
};
PRUnichar nsHtml5NamedCharacters::NAME_1239[] = {
  'l', 'a', 'r', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1239[] = {
  0x2190
};
PRUnichar nsHtml5NamedCharacters::NAME_1240[] = {
  'l', 'a', 'r', 'r', 'b', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1240[] = {
  0x21e4
};
PRUnichar nsHtml5NamedCharacters::NAME_1241[] = {
  'l', 'a', 'r', 'r', 'b', 'f', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1241[] = {
  0x291f
};
PRUnichar nsHtml5NamedCharacters::NAME_1242[] = {
  'l', 'a', 'r', 'r', 'f', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1242[] = {
  0x291d
};
PRUnichar nsHtml5NamedCharacters::NAME_1243[] = {
  'l', 'a', 'r', 'r', 'h', 'k', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1243[] = {
  0x21a9
};
PRUnichar nsHtml5NamedCharacters::NAME_1244[] = {
  'l', 'a', 'r', 'r', 'l', 'p', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1244[] = {
  0x21ab
};
PRUnichar nsHtml5NamedCharacters::NAME_1245[] = {
  'l', 'a', 'r', 'r', 'p', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1245[] = {
  0x2939
};
PRUnichar nsHtml5NamedCharacters::NAME_1246[] = {
  'l', 'a', 'r', 'r', 's', 'i', 'm', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1246[] = {
  0x2973
};
PRUnichar nsHtml5NamedCharacters::NAME_1247[] = {
  'l', 'a', 'r', 'r', 't', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1247[] = {
  0x21a2
};
PRUnichar nsHtml5NamedCharacters::NAME_1248[] = {
  'l', 'a', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1248[] = {
  0x2aab
};
PRUnichar nsHtml5NamedCharacters::NAME_1249[] = {
  'l', 'a', 't', 'a', 'i', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1249[] = {
  0x2919
};
PRUnichar nsHtml5NamedCharacters::NAME_1250[] = {
  'l', 'a', 't', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1250[] = {
  0x2aad
};
PRUnichar nsHtml5NamedCharacters::NAME_1251[] = {
  'l', 'b', 'a', 'r', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1251[] = {
  0x290c
};
PRUnichar nsHtml5NamedCharacters::NAME_1252[] = {
  'l', 'b', 'b', 'r', 'k', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1252[] = {
  0x2772
};
PRUnichar nsHtml5NamedCharacters::NAME_1253[] = {
  'l', 'b', 'r', 'a', 'c', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1253[] = {
  0x007b
};
PRUnichar nsHtml5NamedCharacters::NAME_1254[] = {
  'l', 'b', 'r', 'a', 'c', 'k', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1254[] = {
  0x005b
};
PRUnichar nsHtml5NamedCharacters::NAME_1255[] = {
  'l', 'b', 'r', 'k', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1255[] = {
  0x298b
};
PRUnichar nsHtml5NamedCharacters::NAME_1256[] = {
  'l', 'b', 'r', 'k', 's', 'l', 'd', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1256[] = {
  0x298f
};
PRUnichar nsHtml5NamedCharacters::NAME_1257[] = {
  'l', 'b', 'r', 'k', 's', 'l', 'u', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1257[] = {
  0x298d
};
PRUnichar nsHtml5NamedCharacters::NAME_1258[] = {
  'l', 'c', 'a', 'r', 'o', 'n', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1258[] = {
  0x013e
};
PRUnichar nsHtml5NamedCharacters::NAME_1259[] = {
  'l', 'c', 'e', 'd', 'i', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1259[] = {
  0x013c
};
PRUnichar nsHtml5NamedCharacters::NAME_1260[] = {
  'l', 'c', 'e', 'i', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1260[] = {
  0x2308
};
PRUnichar nsHtml5NamedCharacters::NAME_1261[] = {
  'l', 'c', 'u', 'b', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1261[] = {
  0x007b
};
PRUnichar nsHtml5NamedCharacters::NAME_1262[] = {
  'l', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1262[] = {
  0x043b
};
PRUnichar nsHtml5NamedCharacters::NAME_1263[] = {
  'l', 'd', 'c', 'a', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1263[] = {
  0x2936
};
PRUnichar nsHtml5NamedCharacters::NAME_1264[] = {
  'l', 'd', 'q', 'u', 'o', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1264[] = {
  0x201c
};
PRUnichar nsHtml5NamedCharacters::NAME_1265[] = {
  'l', 'd', 'q', 'u', 'o', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1265[] = {
  0x201e
};
PRUnichar nsHtml5NamedCharacters::NAME_1266[] = {
  'l', 'd', 'r', 'd', 'h', 'a', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1266[] = {
  0x2967
};
PRUnichar nsHtml5NamedCharacters::NAME_1267[] = {
  'l', 'd', 'r', 'u', 's', 'h', 'a', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1267[] = {
  0x294b
};
PRUnichar nsHtml5NamedCharacters::NAME_1268[] = {
  'l', 'd', 's', 'h', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1268[] = {
  0x21b2
};
PRUnichar nsHtml5NamedCharacters::NAME_1269[] = {
  'l', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1269[] = {
  0x2264
};
PRUnichar nsHtml5NamedCharacters::NAME_1270[] = {
  'l', 'e', 'f', 't', 'a', 'r', 'r', 'o', 'w', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1270[] = {
  0x2190
};
PRUnichar nsHtml5NamedCharacters::NAME_1271[] = {
  'l', 'e', 'f', 't', 'a', 'r', 'r', 'o', 'w', 't', 'a', 'i', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1271[] = {
  0x21a2
};
PRUnichar nsHtml5NamedCharacters::NAME_1272[] = {
  'l', 'e', 'f', 't', 'h', 'a', 'r', 'p', 'o', 'o', 'n', 'd', 'o', 'w', 'n', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1272[] = {
  0x21bd
};
PRUnichar nsHtml5NamedCharacters::NAME_1273[] = {
  'l', 'e', 'f', 't', 'h', 'a', 'r', 'p', 'o', 'o', 'n', 'u', 'p', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1273[] = {
  0x21bc
};
PRUnichar nsHtml5NamedCharacters::NAME_1274[] = {
  'l', 'e', 'f', 't', 'l', 'e', 'f', 't', 'a', 'r', 'r', 'o', 'w', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1274[] = {
  0x21c7
};
PRUnichar nsHtml5NamedCharacters::NAME_1275[] = {
  'l', 'e', 'f', 't', 'r', 'i', 'g', 'h', 't', 'a', 'r', 'r', 'o', 'w', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1275[] = {
  0x2194
};
PRUnichar nsHtml5NamedCharacters::NAME_1276[] = {
  'l', 'e', 'f', 't', 'r', 'i', 'g', 'h', 't', 'a', 'r', 'r', 'o', 'w', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1276[] = {
  0x21c6
};
PRUnichar nsHtml5NamedCharacters::NAME_1277[] = {
  'l', 'e', 'f', 't', 'r', 'i', 'g', 'h', 't', 'h', 'a', 'r', 'p', 'o', 'o', 'n', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1277[] = {
  0x21cb
};
PRUnichar nsHtml5NamedCharacters::NAME_1278[] = {
  'l', 'e', 'f', 't', 'r', 'i', 'g', 'h', 't', 's', 'q', 'u', 'i', 'g', 'a', 'r', 'r', 'o', 'w', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1278[] = {
  0x21ad
};
PRUnichar nsHtml5NamedCharacters::NAME_1279[] = {
  'l', 'e', 'f', 't', 't', 'h', 'r', 'e', 'e', 't', 'i', 'm', 'e', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1279[] = {
  0x22cb
};
PRUnichar nsHtml5NamedCharacters::NAME_1280[] = {
  'l', 'e', 'g', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1280[] = {
  0x22da
};
PRUnichar nsHtml5NamedCharacters::NAME_1281[] = {
  'l', 'e', 'q', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1281[] = {
  0x2264
};
PRUnichar nsHtml5NamedCharacters::NAME_1282[] = {
  'l', 'e', 'q', 'q', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1282[] = {
  0x2266
};
PRUnichar nsHtml5NamedCharacters::NAME_1283[] = {
  'l', 'e', 'q', 's', 'l', 'a', 'n', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1283[] = {
  0x2a7d
};
PRUnichar nsHtml5NamedCharacters::NAME_1284[] = {
  'l', 'e', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1284[] = {
  0x2a7d
};
PRUnichar nsHtml5NamedCharacters::NAME_1285[] = {
  'l', 'e', 's', 'c', 'c', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1285[] = {
  0x2aa8
};
PRUnichar nsHtml5NamedCharacters::NAME_1286[] = {
  'l', 'e', 's', 'd', 'o', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1286[] = {
  0x2a7f
};
PRUnichar nsHtml5NamedCharacters::NAME_1287[] = {
  'l', 'e', 's', 'd', 'o', 't', 'o', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1287[] = {
  0x2a81
};
PRUnichar nsHtml5NamedCharacters::NAME_1288[] = {
  'l', 'e', 's', 'd', 'o', 't', 'o', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1288[] = {
  0x2a83
};
PRUnichar nsHtml5NamedCharacters::NAME_1289[] = {
  'l', 'e', 's', 'g', 'e', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1289[] = {
  0x2a93
};
PRUnichar nsHtml5NamedCharacters::NAME_1290[] = {
  'l', 'e', 's', 's', 'a', 'p', 'p', 'r', 'o', 'x', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1290[] = {
  0x2a85
};
PRUnichar nsHtml5NamedCharacters::NAME_1291[] = {
  'l', 'e', 's', 's', 'd', 'o', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1291[] = {
  0x22d6
};
PRUnichar nsHtml5NamedCharacters::NAME_1292[] = {
  'l', 'e', 's', 's', 'e', 'q', 'g', 't', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1292[] = {
  0x22da
};
PRUnichar nsHtml5NamedCharacters::NAME_1293[] = {
  'l', 'e', 's', 's', 'e', 'q', 'q', 'g', 't', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1293[] = {
  0x2a8b
};
PRUnichar nsHtml5NamedCharacters::NAME_1294[] = {
  'l', 'e', 's', 's', 'g', 't', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1294[] = {
  0x2276
};
PRUnichar nsHtml5NamedCharacters::NAME_1295[] = {
  'l', 'e', 's', 's', 's', 'i', 'm', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1295[] = {
  0x2272
};
PRUnichar nsHtml5NamedCharacters::NAME_1296[] = {
  'l', 'f', 'i', 's', 'h', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1296[] = {
  0x297c
};
PRUnichar nsHtml5NamedCharacters::NAME_1297[] = {
  'l', 'f', 'l', 'o', 'o', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1297[] = {
  0x230a
};
PRUnichar nsHtml5NamedCharacters::NAME_1298[] = {
  'l', 'f', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1298[] = {
  0xd835, 0xdd29
};
PRUnichar nsHtml5NamedCharacters::NAME_1299[] = {
  'l', 'g', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1299[] = {
  0x2276
};
PRUnichar nsHtml5NamedCharacters::NAME_1300[] = {
  'l', 'g', 'E', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1300[] = {
  0x2a91
};
PRUnichar nsHtml5NamedCharacters::NAME_1301[] = {
  'l', 'h', 'a', 'r', 'd', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1301[] = {
  0x21bd
};
PRUnichar nsHtml5NamedCharacters::NAME_1302[] = {
  'l', 'h', 'a', 'r', 'u', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1302[] = {
  0x21bc
};
PRUnichar nsHtml5NamedCharacters::NAME_1303[] = {
  'l', 'h', 'a', 'r', 'u', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1303[] = {
  0x296a
};
PRUnichar nsHtml5NamedCharacters::NAME_1304[] = {
  'l', 'h', 'b', 'l', 'k', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1304[] = {
  0x2584
};
PRUnichar nsHtml5NamedCharacters::NAME_1305[] = {
  'l', 'j', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1305[] = {
  0x0459
};
PRUnichar nsHtml5NamedCharacters::NAME_1306[] = {
  'l', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1306[] = {
  0x226a
};
PRUnichar nsHtml5NamedCharacters::NAME_1307[] = {
  'l', 'l', 'a', 'r', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1307[] = {
  0x21c7
};
PRUnichar nsHtml5NamedCharacters::NAME_1308[] = {
  'l', 'l', 'c', 'o', 'r', 'n', 'e', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1308[] = {
  0x231e
};
PRUnichar nsHtml5NamedCharacters::NAME_1309[] = {
  'l', 'l', 'h', 'a', 'r', 'd', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1309[] = {
  0x296b
};
PRUnichar nsHtml5NamedCharacters::NAME_1310[] = {
  'l', 'l', 't', 'r', 'i', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1310[] = {
  0x25fa
};
PRUnichar nsHtml5NamedCharacters::NAME_1311[] = {
  'l', 'm', 'i', 'd', 'o', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1311[] = {
  0x0140
};
PRUnichar nsHtml5NamedCharacters::NAME_1312[] = {
  'l', 'm', 'o', 'u', 's', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1312[] = {
  0x23b0
};
PRUnichar nsHtml5NamedCharacters::NAME_1313[] = {
  'l', 'm', 'o', 'u', 's', 't', 'a', 'c', 'h', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1313[] = {
  0x23b0
};
PRUnichar nsHtml5NamedCharacters::NAME_1314[] = {
  'l', 'n', 'E', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1314[] = {
  0x2268
};
PRUnichar nsHtml5NamedCharacters::NAME_1315[] = {
  'l', 'n', 'a', 'p', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1315[] = {
  0x2a89
};
PRUnichar nsHtml5NamedCharacters::NAME_1316[] = {
  'l', 'n', 'a', 'p', 'p', 'r', 'o', 'x', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1316[] = {
  0x2a89
};
PRUnichar nsHtml5NamedCharacters::NAME_1317[] = {
  'l', 'n', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1317[] = {
  0x2a87
};
PRUnichar nsHtml5NamedCharacters::NAME_1318[] = {
  'l', 'n', 'e', 'q', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1318[] = {
  0x2a87
};
PRUnichar nsHtml5NamedCharacters::NAME_1319[] = {
  'l', 'n', 'e', 'q', 'q', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1319[] = {
  0x2268
};
PRUnichar nsHtml5NamedCharacters::NAME_1320[] = {
  'l', 'n', 's', 'i', 'm', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1320[] = {
  0x22e6
};
PRUnichar nsHtml5NamedCharacters::NAME_1321[] = {
  'l', 'o', 'a', 'n', 'g', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1321[] = {
  0x27ec
};
PRUnichar nsHtml5NamedCharacters::NAME_1322[] = {
  'l', 'o', 'a', 'r', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1322[] = {
  0x21fd
};
PRUnichar nsHtml5NamedCharacters::NAME_1323[] = {
  'l', 'o', 'b', 'r', 'k', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1323[] = {
  0x27e6
};
PRUnichar nsHtml5NamedCharacters::NAME_1324[] = {
  'l', 'o', 'n', 'g', 'l', 'e', 'f', 't', 'a', 'r', 'r', 'o', 'w', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1324[] = {
  0x27f5
};
PRUnichar nsHtml5NamedCharacters::NAME_1325[] = {
  'l', 'o', 'n', 'g', 'l', 'e', 'f', 't', 'r', 'i', 'g', 'h', 't', 'a', 'r', 'r', 'o', 'w', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1325[] = {
  0x27f7
};
PRUnichar nsHtml5NamedCharacters::NAME_1326[] = {
  'l', 'o', 'n', 'g', 'm', 'a', 'p', 's', 't', 'o', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1326[] = {
  0x27fc
};
PRUnichar nsHtml5NamedCharacters::NAME_1327[] = {
  'l', 'o', 'n', 'g', 'r', 'i', 'g', 'h', 't', 'a', 'r', 'r', 'o', 'w', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1327[] = {
  0x27f6
};
PRUnichar nsHtml5NamedCharacters::NAME_1328[] = {
  'l', 'o', 'o', 'p', 'a', 'r', 'r', 'o', 'w', 'l', 'e', 'f', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1328[] = {
  0x21ab
};
PRUnichar nsHtml5NamedCharacters::NAME_1329[] = {
  'l', 'o', 'o', 'p', 'a', 'r', 'r', 'o', 'w', 'r', 'i', 'g', 'h', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1329[] = {
  0x21ac
};
PRUnichar nsHtml5NamedCharacters::NAME_1330[] = {
  'l', 'o', 'p', 'a', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1330[] = {
  0x2985
};
PRUnichar nsHtml5NamedCharacters::NAME_1331[] = {
  'l', 'o', 'p', 'f', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1331[] = {
  0xd835, 0xdd5d
};
PRUnichar nsHtml5NamedCharacters::NAME_1332[] = {
  'l', 'o', 'p', 'l', 'u', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1332[] = {
  0x2a2d
};
PRUnichar nsHtml5NamedCharacters::NAME_1333[] = {
  'l', 'o', 't', 'i', 'm', 'e', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1333[] = {
  0x2a34
};
PRUnichar nsHtml5NamedCharacters::NAME_1334[] = {
  'l', 'o', 'w', 'a', 's', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1334[] = {
  0x2217
};
PRUnichar nsHtml5NamedCharacters::NAME_1335[] = {
  'l', 'o', 'w', 'b', 'a', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1335[] = {
  0x005f
};
PRUnichar nsHtml5NamedCharacters::NAME_1336[] = {
  'l', 'o', 'z', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1336[] = {
  0x25ca
};
PRUnichar nsHtml5NamedCharacters::NAME_1337[] = {
  'l', 'o', 'z', 'e', 'n', 'g', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1337[] = {
  0x25ca
};
PRUnichar nsHtml5NamedCharacters::NAME_1338[] = {
  'l', 'o', 'z', 'f', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1338[] = {
  0x29eb
};
PRUnichar nsHtml5NamedCharacters::NAME_1339[] = {
  'l', 'p', 'a', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1339[] = {
  0x0028
};
PRUnichar nsHtml5NamedCharacters::NAME_1340[] = {
  'l', 'p', 'a', 'r', 'l', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1340[] = {
  0x2993
};
PRUnichar nsHtml5NamedCharacters::NAME_1341[] = {
  'l', 'r', 'a', 'r', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1341[] = {
  0x21c6
};
PRUnichar nsHtml5NamedCharacters::NAME_1342[] = {
  'l', 'r', 'c', 'o', 'r', 'n', 'e', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1342[] = {
  0x231f
};
PRUnichar nsHtml5NamedCharacters::NAME_1343[] = {
  'l', 'r', 'h', 'a', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1343[] = {
  0x21cb
};
PRUnichar nsHtml5NamedCharacters::NAME_1344[] = {
  'l', 'r', 'h', 'a', 'r', 'd', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1344[] = {
  0x296d
};
PRUnichar nsHtml5NamedCharacters::NAME_1345[] = {
  'l', 'r', 'm', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1345[] = {
  0x200e
};
PRUnichar nsHtml5NamedCharacters::NAME_1346[] = {
  'l', 'r', 't', 'r', 'i', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1346[] = {
  0x22bf
};
PRUnichar nsHtml5NamedCharacters::NAME_1347[] = {
  'l', 's', 'a', 'q', 'u', 'o', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1347[] = {
  0x2039
};
PRUnichar nsHtml5NamedCharacters::NAME_1348[] = {
  'l', 's', 'c', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1348[] = {
  0xd835, 0xdcc1
};
PRUnichar nsHtml5NamedCharacters::NAME_1349[] = {
  'l', 's', 'h', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1349[] = {
  0x21b0
};
PRUnichar nsHtml5NamedCharacters::NAME_1350[] = {
  'l', 's', 'i', 'm', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1350[] = {
  0x2272
};
PRUnichar nsHtml5NamedCharacters::NAME_1351[] = {
  'l', 's', 'i', 'm', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1351[] = {
  0x2a8d
};
PRUnichar nsHtml5NamedCharacters::NAME_1352[] = {
  'l', 's', 'i', 'm', 'g', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1352[] = {
  0x2a8f
};
PRUnichar nsHtml5NamedCharacters::NAME_1353[] = {
  'l', 's', 'q', 'b', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1353[] = {
  0x005b
};
PRUnichar nsHtml5NamedCharacters::NAME_1354[] = {
  'l', 's', 'q', 'u', 'o', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1354[] = {
  0x2018
};
PRUnichar nsHtml5NamedCharacters::NAME_1355[] = {
  'l', 's', 'q', 'u', 'o', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1355[] = {
  0x201a
};
PRUnichar nsHtml5NamedCharacters::NAME_1356[] = {
  'l', 's', 't', 'r', 'o', 'k', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1356[] = {
  0x0142
};
PRUnichar nsHtml5NamedCharacters::NAME_1357[] = {
  'l', 't'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1357[] = {
  0x003c
};
PRUnichar nsHtml5NamedCharacters::NAME_1358[] = {
  'l', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1358[] = {
  0x003c
};
PRUnichar nsHtml5NamedCharacters::NAME_1359[] = {
  'l', 't', 'c', 'c', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1359[] = {
  0x2aa6
};
PRUnichar nsHtml5NamedCharacters::NAME_1360[] = {
  'l', 't', 'c', 'i', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1360[] = {
  0x2a79
};
PRUnichar nsHtml5NamedCharacters::NAME_1361[] = {
  'l', 't', 'd', 'o', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1361[] = {
  0x22d6
};
PRUnichar nsHtml5NamedCharacters::NAME_1362[] = {
  'l', 't', 'h', 'r', 'e', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1362[] = {
  0x22cb
};
PRUnichar nsHtml5NamedCharacters::NAME_1363[] = {
  'l', 't', 'i', 'm', 'e', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1363[] = {
  0x22c9
};
PRUnichar nsHtml5NamedCharacters::NAME_1364[] = {
  'l', 't', 'l', 'a', 'r', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1364[] = {
  0x2976
};
PRUnichar nsHtml5NamedCharacters::NAME_1365[] = {
  'l', 't', 'q', 'u', 'e', 's', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1365[] = {
  0x2a7b
};
PRUnichar nsHtml5NamedCharacters::NAME_1366[] = {
  'l', 't', 'r', 'P', 'a', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1366[] = {
  0x2996
};
PRUnichar nsHtml5NamedCharacters::NAME_1367[] = {
  'l', 't', 'r', 'i', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1367[] = {
  0x25c3
};
PRUnichar nsHtml5NamedCharacters::NAME_1368[] = {
  'l', 't', 'r', 'i', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1368[] = {
  0x22b4
};
PRUnichar nsHtml5NamedCharacters::NAME_1369[] = {
  'l', 't', 'r', 'i', 'f', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1369[] = {
  0x25c2
};
PRUnichar nsHtml5NamedCharacters::NAME_1370[] = {
  'l', 'u', 'r', 'd', 's', 'h', 'a', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1370[] = {
  0x294a
};
PRUnichar nsHtml5NamedCharacters::NAME_1371[] = {
  'l', 'u', 'r', 'u', 'h', 'a', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1371[] = {
  0x2966
};
PRUnichar nsHtml5NamedCharacters::NAME_1372[] = {
  'm', 'D', 'D', 'o', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1372[] = {
  0x223a
};
PRUnichar nsHtml5NamedCharacters::NAME_1373[] = {
  'm', 'a', 'c', 'r'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1373[] = {
  0x00af
};
PRUnichar nsHtml5NamedCharacters::NAME_1374[] = {
  'm', 'a', 'c', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1374[] = {
  0x00af
};
PRUnichar nsHtml5NamedCharacters::NAME_1375[] = {
  'm', 'a', 'l', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1375[] = {
  0x2642
};
PRUnichar nsHtml5NamedCharacters::NAME_1376[] = {
  'm', 'a', 'l', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1376[] = {
  0x2720
};
PRUnichar nsHtml5NamedCharacters::NAME_1377[] = {
  'm', 'a', 'l', 't', 'e', 's', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1377[] = {
  0x2720
};
PRUnichar nsHtml5NamedCharacters::NAME_1378[] = {
  'm', 'a', 'p', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1378[] = {
  0x21a6
};
PRUnichar nsHtml5NamedCharacters::NAME_1379[] = {
  'm', 'a', 'p', 's', 't', 'o', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1379[] = {
  0x21a6
};
PRUnichar nsHtml5NamedCharacters::NAME_1380[] = {
  'm', 'a', 'p', 's', 't', 'o', 'd', 'o', 'w', 'n', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1380[] = {
  0x21a7
};
PRUnichar nsHtml5NamedCharacters::NAME_1381[] = {
  'm', 'a', 'p', 's', 't', 'o', 'l', 'e', 'f', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1381[] = {
  0x21a4
};
PRUnichar nsHtml5NamedCharacters::NAME_1382[] = {
  'm', 'a', 'p', 's', 't', 'o', 'u', 'p', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1382[] = {
  0x21a5
};
PRUnichar nsHtml5NamedCharacters::NAME_1383[] = {
  'm', 'a', 'r', 'k', 'e', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1383[] = {
  0x25ae
};
PRUnichar nsHtml5NamedCharacters::NAME_1384[] = {
  'm', 'c', 'o', 'm', 'm', 'a', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1384[] = {
  0x2a29
};
PRUnichar nsHtml5NamedCharacters::NAME_1385[] = {
  'm', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1385[] = {
  0x043c
};
PRUnichar nsHtml5NamedCharacters::NAME_1386[] = {
  'm', 'd', 'a', 's', 'h', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1386[] = {
  0x2014
};
PRUnichar nsHtml5NamedCharacters::NAME_1387[] = {
  'm', 'e', 'a', 's', 'u', 'r', 'e', 'd', 'a', 'n', 'g', 'l', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1387[] = {
  0x2221
};
PRUnichar nsHtml5NamedCharacters::NAME_1388[] = {
  'm', 'f', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1388[] = {
  0xd835, 0xdd2a
};
PRUnichar nsHtml5NamedCharacters::NAME_1389[] = {
  'm', 'h', 'o', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1389[] = {
  0x2127
};
PRUnichar nsHtml5NamedCharacters::NAME_1390[] = {
  'm', 'i', 'c', 'r', 'o'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1390[] = {
  0x00b5
};
PRUnichar nsHtml5NamedCharacters::NAME_1391[] = {
  'm', 'i', 'c', 'r', 'o', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1391[] = {
  0x00b5
};
PRUnichar nsHtml5NamedCharacters::NAME_1392[] = {
  'm', 'i', 'd', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1392[] = {
  0x2223
};
PRUnichar nsHtml5NamedCharacters::NAME_1393[] = {
  'm', 'i', 'd', 'a', 's', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1393[] = {
  0x002a
};
PRUnichar nsHtml5NamedCharacters::NAME_1394[] = {
  'm', 'i', 'd', 'c', 'i', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1394[] = {
  0x2af0
};
PRUnichar nsHtml5NamedCharacters::NAME_1395[] = {
  'm', 'i', 'd', 'd', 'o', 't'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1395[] = {
  0x00b7
};
PRUnichar nsHtml5NamedCharacters::NAME_1396[] = {
  'm', 'i', 'd', 'd', 'o', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1396[] = {
  0x00b7
};
PRUnichar nsHtml5NamedCharacters::NAME_1397[] = {
  'm', 'i', 'n', 'u', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1397[] = {
  0x2212
};
PRUnichar nsHtml5NamedCharacters::NAME_1398[] = {
  'm', 'i', 'n', 'u', 's', 'b', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1398[] = {
  0x229f
};
PRUnichar nsHtml5NamedCharacters::NAME_1399[] = {
  'm', 'i', 'n', 'u', 's', 'd', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1399[] = {
  0x2238
};
PRUnichar nsHtml5NamedCharacters::NAME_1400[] = {
  'm', 'i', 'n', 'u', 's', 'd', 'u', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1400[] = {
  0x2a2a
};
PRUnichar nsHtml5NamedCharacters::NAME_1401[] = {
  'm', 'l', 'c', 'p', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1401[] = {
  0x2adb
};
PRUnichar nsHtml5NamedCharacters::NAME_1402[] = {
  'm', 'l', 'd', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1402[] = {
  0x2026
};
PRUnichar nsHtml5NamedCharacters::NAME_1403[] = {
  'm', 'n', 'p', 'l', 'u', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1403[] = {
  0x2213
};
PRUnichar nsHtml5NamedCharacters::NAME_1404[] = {
  'm', 'o', 'd', 'e', 'l', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1404[] = {
  0x22a7
};
PRUnichar nsHtml5NamedCharacters::NAME_1405[] = {
  'm', 'o', 'p', 'f', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1405[] = {
  0xd835, 0xdd5e
};
PRUnichar nsHtml5NamedCharacters::NAME_1406[] = {
  'm', 'p', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1406[] = {
  0x2213
};
PRUnichar nsHtml5NamedCharacters::NAME_1407[] = {
  'm', 's', 'c', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1407[] = {
  0xd835, 0xdcc2
};
PRUnichar nsHtml5NamedCharacters::NAME_1408[] = {
  'm', 's', 't', 'p', 'o', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1408[] = {
  0x223e
};
PRUnichar nsHtml5NamedCharacters::NAME_1409[] = {
  'm', 'u', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1409[] = {
  0x03bc
};
PRUnichar nsHtml5NamedCharacters::NAME_1410[] = {
  'm', 'u', 'l', 't', 'i', 'm', 'a', 'p', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1410[] = {
  0x22b8
};
PRUnichar nsHtml5NamedCharacters::NAME_1411[] = {
  'm', 'u', 'm', 'a', 'p', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1411[] = {
  0x22b8
};
PRUnichar nsHtml5NamedCharacters::NAME_1412[] = {
  'n', 'L', 'e', 'f', 't', 'a', 'r', 'r', 'o', 'w', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1412[] = {
  0x21cd
};
PRUnichar nsHtml5NamedCharacters::NAME_1413[] = {
  'n', 'L', 'e', 'f', 't', 'r', 'i', 'g', 'h', 't', 'a', 'r', 'r', 'o', 'w', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1413[] = {
  0x21ce
};
PRUnichar nsHtml5NamedCharacters::NAME_1414[] = {
  'n', 'R', 'i', 'g', 'h', 't', 'a', 'r', 'r', 'o', 'w', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1414[] = {
  0x21cf
};
PRUnichar nsHtml5NamedCharacters::NAME_1415[] = {
  'n', 'V', 'D', 'a', 's', 'h', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1415[] = {
  0x22af
};
PRUnichar nsHtml5NamedCharacters::NAME_1416[] = {
  'n', 'V', 'd', 'a', 's', 'h', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1416[] = {
  0x22ae
};
PRUnichar nsHtml5NamedCharacters::NAME_1417[] = {
  'n', 'a', 'b', 'l', 'a', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1417[] = {
  0x2207
};
PRUnichar nsHtml5NamedCharacters::NAME_1418[] = {
  'n', 'a', 'c', 'u', 't', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1418[] = {
  0x0144
};
PRUnichar nsHtml5NamedCharacters::NAME_1419[] = {
  'n', 'a', 'p', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1419[] = {
  0x2249
};
PRUnichar nsHtml5NamedCharacters::NAME_1420[] = {
  'n', 'a', 'p', 'o', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1420[] = {
  0x0149
};
PRUnichar nsHtml5NamedCharacters::NAME_1421[] = {
  'n', 'a', 'p', 'p', 'r', 'o', 'x', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1421[] = {
  0x2249
};
PRUnichar nsHtml5NamedCharacters::NAME_1422[] = {
  'n', 'a', 't', 'u', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1422[] = {
  0x266e
};
PRUnichar nsHtml5NamedCharacters::NAME_1423[] = {
  'n', 'a', 't', 'u', 'r', 'a', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1423[] = {
  0x266e
};
PRUnichar nsHtml5NamedCharacters::NAME_1424[] = {
  'n', 'a', 't', 'u', 'r', 'a', 'l', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1424[] = {
  0x2115
};
PRUnichar nsHtml5NamedCharacters::NAME_1425[] = {
  'n', 'b', 's', 'p'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1425[] = {
  0x00a0
};
PRUnichar nsHtml5NamedCharacters::NAME_1426[] = {
  'n', 'b', 's', 'p', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1426[] = {
  0x00a0
};
PRUnichar nsHtml5NamedCharacters::NAME_1427[] = {
  'n', 'c', 'a', 'p', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1427[] = {
  0x2a43
};
PRUnichar nsHtml5NamedCharacters::NAME_1428[] = {
  'n', 'c', 'a', 'r', 'o', 'n', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1428[] = {
  0x0148
};
PRUnichar nsHtml5NamedCharacters::NAME_1429[] = {
  'n', 'c', 'e', 'd', 'i', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1429[] = {
  0x0146
};
PRUnichar nsHtml5NamedCharacters::NAME_1430[] = {
  'n', 'c', 'o', 'n', 'g', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1430[] = {
  0x2247
};
PRUnichar nsHtml5NamedCharacters::NAME_1431[] = {
  'n', 'c', 'u', 'p', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1431[] = {
  0x2a42
};
PRUnichar nsHtml5NamedCharacters::NAME_1432[] = {
  'n', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1432[] = {
  0x043d
};
PRUnichar nsHtml5NamedCharacters::NAME_1433[] = {
  'n', 'd', 'a', 's', 'h', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1433[] = {
  0x2013
};
PRUnichar nsHtml5NamedCharacters::NAME_1434[] = {
  'n', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1434[] = {
  0x2260
};
PRUnichar nsHtml5NamedCharacters::NAME_1435[] = {
  'n', 'e', 'A', 'r', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1435[] = {
  0x21d7
};
PRUnichar nsHtml5NamedCharacters::NAME_1436[] = {
  'n', 'e', 'a', 'r', 'h', 'k', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1436[] = {
  0x2924
};
PRUnichar nsHtml5NamedCharacters::NAME_1437[] = {
  'n', 'e', 'a', 'r', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1437[] = {
  0x2197
};
PRUnichar nsHtml5NamedCharacters::NAME_1438[] = {
  'n', 'e', 'a', 'r', 'r', 'o', 'w', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1438[] = {
  0x2197
};
PRUnichar nsHtml5NamedCharacters::NAME_1439[] = {
  'n', 'e', 'q', 'u', 'i', 'v', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1439[] = {
  0x2262
};
PRUnichar nsHtml5NamedCharacters::NAME_1440[] = {
  'n', 'e', 's', 'e', 'a', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1440[] = {
  0x2928
};
PRUnichar nsHtml5NamedCharacters::NAME_1441[] = {
  'n', 'e', 'x', 'i', 's', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1441[] = {
  0x2204
};
PRUnichar nsHtml5NamedCharacters::NAME_1442[] = {
  'n', 'e', 'x', 'i', 's', 't', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1442[] = {
  0x2204
};
PRUnichar nsHtml5NamedCharacters::NAME_1443[] = {
  'n', 'f', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1443[] = {
  0xd835, 0xdd2b
};
PRUnichar nsHtml5NamedCharacters::NAME_1444[] = {
  'n', 'g', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1444[] = {
  0x2271
};
PRUnichar nsHtml5NamedCharacters::NAME_1445[] = {
  'n', 'g', 'e', 'q', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1445[] = {
  0x2271
};
PRUnichar nsHtml5NamedCharacters::NAME_1446[] = {
  'n', 'g', 's', 'i', 'm', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1446[] = {
  0x2275
};
PRUnichar nsHtml5NamedCharacters::NAME_1447[] = {
  'n', 'g', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1447[] = {
  0x226f
};
PRUnichar nsHtml5NamedCharacters::NAME_1448[] = {
  'n', 'g', 't', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1448[] = {
  0x226f
};
PRUnichar nsHtml5NamedCharacters::NAME_1449[] = {
  'n', 'h', 'A', 'r', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1449[] = {
  0x21ce
};
PRUnichar nsHtml5NamedCharacters::NAME_1450[] = {
  'n', 'h', 'a', 'r', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1450[] = {
  0x21ae
};
PRUnichar nsHtml5NamedCharacters::NAME_1451[] = {
  'n', 'h', 'p', 'a', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1451[] = {
  0x2af2
};
PRUnichar nsHtml5NamedCharacters::NAME_1452[] = {
  'n', 'i', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1452[] = {
  0x220b
};
PRUnichar nsHtml5NamedCharacters::NAME_1453[] = {
  'n', 'i', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1453[] = {
  0x22fc
};
PRUnichar nsHtml5NamedCharacters::NAME_1454[] = {
  'n', 'i', 's', 'd', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1454[] = {
  0x22fa
};
PRUnichar nsHtml5NamedCharacters::NAME_1455[] = {
  'n', 'i', 'v', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1455[] = {
  0x220b
};
PRUnichar nsHtml5NamedCharacters::NAME_1456[] = {
  'n', 'j', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1456[] = {
  0x045a
};
PRUnichar nsHtml5NamedCharacters::NAME_1457[] = {
  'n', 'l', 'A', 'r', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1457[] = {
  0x21cd
};
PRUnichar nsHtml5NamedCharacters::NAME_1458[] = {
  'n', 'l', 'a', 'r', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1458[] = {
  0x219a
};
PRUnichar nsHtml5NamedCharacters::NAME_1459[] = {
  'n', 'l', 'd', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1459[] = {
  0x2025
};
PRUnichar nsHtml5NamedCharacters::NAME_1460[] = {
  'n', 'l', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1460[] = {
  0x2270
};
PRUnichar nsHtml5NamedCharacters::NAME_1461[] = {
  'n', 'l', 'e', 'f', 't', 'a', 'r', 'r', 'o', 'w', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1461[] = {
  0x219a
};
PRUnichar nsHtml5NamedCharacters::NAME_1462[] = {
  'n', 'l', 'e', 'f', 't', 'r', 'i', 'g', 'h', 't', 'a', 'r', 'r', 'o', 'w', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1462[] = {
  0x21ae
};
PRUnichar nsHtml5NamedCharacters::NAME_1463[] = {
  'n', 'l', 'e', 'q', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1463[] = {
  0x2270
};
PRUnichar nsHtml5NamedCharacters::NAME_1464[] = {
  'n', 'l', 'e', 's', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1464[] = {
  0x226e
};
PRUnichar nsHtml5NamedCharacters::NAME_1465[] = {
  'n', 'l', 's', 'i', 'm', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1465[] = {
  0x2274
};
PRUnichar nsHtml5NamedCharacters::NAME_1466[] = {
  'n', 'l', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1466[] = {
  0x226e
};
PRUnichar nsHtml5NamedCharacters::NAME_1467[] = {
  'n', 'l', 't', 'r', 'i', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1467[] = {
  0x22ea
};
PRUnichar nsHtml5NamedCharacters::NAME_1468[] = {
  'n', 'l', 't', 'r', 'i', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1468[] = {
  0x22ec
};
PRUnichar nsHtml5NamedCharacters::NAME_1469[] = {
  'n', 'm', 'i', 'd', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1469[] = {
  0x2224
};
PRUnichar nsHtml5NamedCharacters::NAME_1470[] = {
  'n', 'o', 'p', 'f', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1470[] = {
  0xd835, 0xdd5f
};
PRUnichar nsHtml5NamedCharacters::NAME_1471[] = {
  'n', 'o', 't'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1471[] = {
  0x00ac
};
PRUnichar nsHtml5NamedCharacters::NAME_1472[] = {
  'n', 'o', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1472[] = {
  0x00ac
};
PRUnichar nsHtml5NamedCharacters::NAME_1473[] = {
  'n', 'o', 't', 'i', 'n', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1473[] = {
  0x2209
};
PRUnichar nsHtml5NamedCharacters::NAME_1474[] = {
  'n', 'o', 't', 'i', 'n', 'v', 'a', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1474[] = {
  0x2209
};
PRUnichar nsHtml5NamedCharacters::NAME_1475[] = {
  'n', 'o', 't', 'i', 'n', 'v', 'b', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1475[] = {
  0x22f7
};
PRUnichar nsHtml5NamedCharacters::NAME_1476[] = {
  'n', 'o', 't', 'i', 'n', 'v', 'c', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1476[] = {
  0x22f6
};
PRUnichar nsHtml5NamedCharacters::NAME_1477[] = {
  'n', 'o', 't', 'n', 'i', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1477[] = {
  0x220c
};
PRUnichar nsHtml5NamedCharacters::NAME_1478[] = {
  'n', 'o', 't', 'n', 'i', 'v', 'a', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1478[] = {
  0x220c
};
PRUnichar nsHtml5NamedCharacters::NAME_1479[] = {
  'n', 'o', 't', 'n', 'i', 'v', 'b', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1479[] = {
  0x22fe
};
PRUnichar nsHtml5NamedCharacters::NAME_1480[] = {
  'n', 'o', 't', 'n', 'i', 'v', 'c', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1480[] = {
  0x22fd
};
PRUnichar nsHtml5NamedCharacters::NAME_1481[] = {
  'n', 'p', 'a', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1481[] = {
  0x2226
};
PRUnichar nsHtml5NamedCharacters::NAME_1482[] = {
  'n', 'p', 'a', 'r', 'a', 'l', 'l', 'e', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1482[] = {
  0x2226
};
PRUnichar nsHtml5NamedCharacters::NAME_1483[] = {
  'n', 'p', 'o', 'l', 'i', 'n', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1483[] = {
  0x2a14
};
PRUnichar nsHtml5NamedCharacters::NAME_1484[] = {
  'n', 'p', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1484[] = {
  0x2280
};
PRUnichar nsHtml5NamedCharacters::NAME_1485[] = {
  'n', 'p', 'r', 'c', 'u', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1485[] = {
  0x22e0
};
PRUnichar nsHtml5NamedCharacters::NAME_1486[] = {
  'n', 'p', 'r', 'e', 'c', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1486[] = {
  0x2280
};
PRUnichar nsHtml5NamedCharacters::NAME_1487[] = {
  'n', 'r', 'A', 'r', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1487[] = {
  0x21cf
};
PRUnichar nsHtml5NamedCharacters::NAME_1488[] = {
  'n', 'r', 'a', 'r', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1488[] = {
  0x219b
};
PRUnichar nsHtml5NamedCharacters::NAME_1489[] = {
  'n', 'r', 'i', 'g', 'h', 't', 'a', 'r', 'r', 'o', 'w', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1489[] = {
  0x219b
};
PRUnichar nsHtml5NamedCharacters::NAME_1490[] = {
  'n', 'r', 't', 'r', 'i', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1490[] = {
  0x22eb
};
PRUnichar nsHtml5NamedCharacters::NAME_1491[] = {
  'n', 'r', 't', 'r', 'i', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1491[] = {
  0x22ed
};
PRUnichar nsHtml5NamedCharacters::NAME_1492[] = {
  'n', 's', 'c', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1492[] = {
  0x2281
};
PRUnichar nsHtml5NamedCharacters::NAME_1493[] = {
  'n', 's', 'c', 'c', 'u', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1493[] = {
  0x22e1
};
PRUnichar nsHtml5NamedCharacters::NAME_1494[] = {
  'n', 's', 'c', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1494[] = {
  0xd835, 0xdcc3
};
PRUnichar nsHtml5NamedCharacters::NAME_1495[] = {
  'n', 's', 'h', 'o', 'r', 't', 'm', 'i', 'd', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1495[] = {
  0x2224
};
PRUnichar nsHtml5NamedCharacters::NAME_1496[] = {
  'n', 's', 'h', 'o', 'r', 't', 'p', 'a', 'r', 'a', 'l', 'l', 'e', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1496[] = {
  0x2226
};
PRUnichar nsHtml5NamedCharacters::NAME_1497[] = {
  'n', 's', 'i', 'm', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1497[] = {
  0x2241
};
PRUnichar nsHtml5NamedCharacters::NAME_1498[] = {
  'n', 's', 'i', 'm', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1498[] = {
  0x2244
};
PRUnichar nsHtml5NamedCharacters::NAME_1499[] = {
  'n', 's', 'i', 'm', 'e', 'q', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1499[] = {
  0x2244
};
PRUnichar nsHtml5NamedCharacters::NAME_1500[] = {
  'n', 's', 'm', 'i', 'd', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1500[] = {
  0x2224
};
PRUnichar nsHtml5NamedCharacters::NAME_1501[] = {
  'n', 's', 'p', 'a', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1501[] = {
  0x2226
};
PRUnichar nsHtml5NamedCharacters::NAME_1502[] = {
  'n', 's', 'q', 's', 'u', 'b', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1502[] = {
  0x22e2
};
PRUnichar nsHtml5NamedCharacters::NAME_1503[] = {
  'n', 's', 'q', 's', 'u', 'p', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1503[] = {
  0x22e3
};
PRUnichar nsHtml5NamedCharacters::NAME_1504[] = {
  'n', 's', 'u', 'b', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1504[] = {
  0x2284
};
PRUnichar nsHtml5NamedCharacters::NAME_1505[] = {
  'n', 's', 'u', 'b', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1505[] = {
  0x2288
};
PRUnichar nsHtml5NamedCharacters::NAME_1506[] = {
  'n', 's', 'u', 'b', 's', 'e', 't', 'e', 'q', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1506[] = {
  0x2288
};
PRUnichar nsHtml5NamedCharacters::NAME_1507[] = {
  'n', 's', 'u', 'c', 'c', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1507[] = {
  0x2281
};
PRUnichar nsHtml5NamedCharacters::NAME_1508[] = {
  'n', 's', 'u', 'p', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1508[] = {
  0x2285
};
PRUnichar nsHtml5NamedCharacters::NAME_1509[] = {
  'n', 's', 'u', 'p', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1509[] = {
  0x2289
};
PRUnichar nsHtml5NamedCharacters::NAME_1510[] = {
  'n', 's', 'u', 'p', 's', 'e', 't', 'e', 'q', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1510[] = {
  0x2289
};
PRUnichar nsHtml5NamedCharacters::NAME_1511[] = {
  'n', 't', 'g', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1511[] = {
  0x2279
};
PRUnichar nsHtml5NamedCharacters::NAME_1512[] = {
  'n', 't', 'i', 'l', 'd', 'e'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1512[] = {
  0x00f1
};
PRUnichar nsHtml5NamedCharacters::NAME_1513[] = {
  'n', 't', 'i', 'l', 'd', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1513[] = {
  0x00f1
};
PRUnichar nsHtml5NamedCharacters::NAME_1514[] = {
  'n', 't', 'l', 'g', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1514[] = {
  0x2278
};
PRUnichar nsHtml5NamedCharacters::NAME_1515[] = {
  'n', 't', 'r', 'i', 'a', 'n', 'g', 'l', 'e', 'l', 'e', 'f', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1515[] = {
  0x22ea
};
PRUnichar nsHtml5NamedCharacters::NAME_1516[] = {
  'n', 't', 'r', 'i', 'a', 'n', 'g', 'l', 'e', 'l', 'e', 'f', 't', 'e', 'q', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1516[] = {
  0x22ec
};
PRUnichar nsHtml5NamedCharacters::NAME_1517[] = {
  'n', 't', 'r', 'i', 'a', 'n', 'g', 'l', 'e', 'r', 'i', 'g', 'h', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1517[] = {
  0x22eb
};
PRUnichar nsHtml5NamedCharacters::NAME_1518[] = {
  'n', 't', 'r', 'i', 'a', 'n', 'g', 'l', 'e', 'r', 'i', 'g', 'h', 't', 'e', 'q', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1518[] = {
  0x22ed
};
PRUnichar nsHtml5NamedCharacters::NAME_1519[] = {
  'n', 'u', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1519[] = {
  0x03bd
};
PRUnichar nsHtml5NamedCharacters::NAME_1520[] = {
  'n', 'u', 'm', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1520[] = {
  0x0023
};
PRUnichar nsHtml5NamedCharacters::NAME_1521[] = {
  'n', 'u', 'm', 'e', 'r', 'o', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1521[] = {
  0x2116
};
PRUnichar nsHtml5NamedCharacters::NAME_1522[] = {
  'n', 'u', 'm', 's', 'p', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1522[] = {
  0x2007
};
PRUnichar nsHtml5NamedCharacters::NAME_1523[] = {
  'n', 'v', 'D', 'a', 's', 'h', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1523[] = {
  0x22ad
};
PRUnichar nsHtml5NamedCharacters::NAME_1524[] = {
  'n', 'v', 'H', 'a', 'r', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1524[] = {
  0x2904
};
PRUnichar nsHtml5NamedCharacters::NAME_1525[] = {
  'n', 'v', 'd', 'a', 's', 'h', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1525[] = {
  0x22ac
};
PRUnichar nsHtml5NamedCharacters::NAME_1526[] = {
  'n', 'v', 'i', 'n', 'f', 'i', 'n', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1526[] = {
  0x29de
};
PRUnichar nsHtml5NamedCharacters::NAME_1527[] = {
  'n', 'v', 'l', 'A', 'r', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1527[] = {
  0x2902
};
PRUnichar nsHtml5NamedCharacters::NAME_1528[] = {
  'n', 'v', 'r', 'A', 'r', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1528[] = {
  0x2903
};
PRUnichar nsHtml5NamedCharacters::NAME_1529[] = {
  'n', 'w', 'A', 'r', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1529[] = {
  0x21d6
};
PRUnichar nsHtml5NamedCharacters::NAME_1530[] = {
  'n', 'w', 'a', 'r', 'h', 'k', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1530[] = {
  0x2923
};
PRUnichar nsHtml5NamedCharacters::NAME_1531[] = {
  'n', 'w', 'a', 'r', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1531[] = {
  0x2196
};
PRUnichar nsHtml5NamedCharacters::NAME_1532[] = {
  'n', 'w', 'a', 'r', 'r', 'o', 'w', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1532[] = {
  0x2196
};
PRUnichar nsHtml5NamedCharacters::NAME_1533[] = {
  'n', 'w', 'n', 'e', 'a', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1533[] = {
  0x2927
};
PRUnichar nsHtml5NamedCharacters::NAME_1534[] = {
  'o', 'S', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1534[] = {
  0x24c8
};
PRUnichar nsHtml5NamedCharacters::NAME_1535[] = {
  'o', 'a', 'c', 'u', 't', 'e'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1535[] = {
  0x00f3
};
PRUnichar nsHtml5NamedCharacters::NAME_1536[] = {
  'o', 'a', 'c', 'u', 't', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1536[] = {
  0x00f3
};
PRUnichar nsHtml5NamedCharacters::NAME_1537[] = {
  'o', 'a', 's', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1537[] = {
  0x229b
};
PRUnichar nsHtml5NamedCharacters::NAME_1538[] = {
  'o', 'c', 'i', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1538[] = {
  0x229a
};
PRUnichar nsHtml5NamedCharacters::NAME_1539[] = {
  'o', 'c', 'i', 'r', 'c'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1539[] = {
  0x00f4
};
PRUnichar nsHtml5NamedCharacters::NAME_1540[] = {
  'o', 'c', 'i', 'r', 'c', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1540[] = {
  0x00f4
};
PRUnichar nsHtml5NamedCharacters::NAME_1541[] = {
  'o', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1541[] = {
  0x043e
};
PRUnichar nsHtml5NamedCharacters::NAME_1542[] = {
  'o', 'd', 'a', 's', 'h', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1542[] = {
  0x229d
};
PRUnichar nsHtml5NamedCharacters::NAME_1543[] = {
  'o', 'd', 'b', 'l', 'a', 'c', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1543[] = {
  0x0151
};
PRUnichar nsHtml5NamedCharacters::NAME_1544[] = {
  'o', 'd', 'i', 'v', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1544[] = {
  0x2a38
};
PRUnichar nsHtml5NamedCharacters::NAME_1545[] = {
  'o', 'd', 'o', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1545[] = {
  0x2299
};
PRUnichar nsHtml5NamedCharacters::NAME_1546[] = {
  'o', 'd', 's', 'o', 'l', 'd', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1546[] = {
  0x29bc
};
PRUnichar nsHtml5NamedCharacters::NAME_1547[] = {
  'o', 'e', 'l', 'i', 'g', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1547[] = {
  0x0153
};
PRUnichar nsHtml5NamedCharacters::NAME_1548[] = {
  'o', 'f', 'c', 'i', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1548[] = {
  0x29bf
};
PRUnichar nsHtml5NamedCharacters::NAME_1549[] = {
  'o', 'f', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1549[] = {
  0xd835, 0xdd2c
};
PRUnichar nsHtml5NamedCharacters::NAME_1550[] = {
  'o', 'g', 'o', 'n', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1550[] = {
  0x02db
};
PRUnichar nsHtml5NamedCharacters::NAME_1551[] = {
  'o', 'g', 'r', 'a', 'v', 'e'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1551[] = {
  0x00f2
};
PRUnichar nsHtml5NamedCharacters::NAME_1552[] = {
  'o', 'g', 'r', 'a', 'v', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1552[] = {
  0x00f2
};
PRUnichar nsHtml5NamedCharacters::NAME_1553[] = {
  'o', 'g', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1553[] = {
  0x29c1
};
PRUnichar nsHtml5NamedCharacters::NAME_1554[] = {
  'o', 'h', 'b', 'a', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1554[] = {
  0x29b5
};
PRUnichar nsHtml5NamedCharacters::NAME_1555[] = {
  'o', 'h', 'm', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1555[] = {
  0x2126
};
PRUnichar nsHtml5NamedCharacters::NAME_1556[] = {
  'o', 'i', 'n', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1556[] = {
  0x222e
};
PRUnichar nsHtml5NamedCharacters::NAME_1557[] = {
  'o', 'l', 'a', 'r', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1557[] = {
  0x21ba
};
PRUnichar nsHtml5NamedCharacters::NAME_1558[] = {
  'o', 'l', 'c', 'i', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1558[] = {
  0x29be
};
PRUnichar nsHtml5NamedCharacters::NAME_1559[] = {
  'o', 'l', 'c', 'r', 'o', 's', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1559[] = {
  0x29bb
};
PRUnichar nsHtml5NamedCharacters::NAME_1560[] = {
  'o', 'l', 'i', 'n', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1560[] = {
  0x203e
};
PRUnichar nsHtml5NamedCharacters::NAME_1561[] = {
  'o', 'l', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1561[] = {
  0x29c0
};
PRUnichar nsHtml5NamedCharacters::NAME_1562[] = {
  'o', 'm', 'a', 'c', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1562[] = {
  0x014d
};
PRUnichar nsHtml5NamedCharacters::NAME_1563[] = {
  'o', 'm', 'e', 'g', 'a', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1563[] = {
  0x03c9
};
PRUnichar nsHtml5NamedCharacters::NAME_1564[] = {
  'o', 'm', 'i', 'c', 'r', 'o', 'n', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1564[] = {
  0x03bf
};
PRUnichar nsHtml5NamedCharacters::NAME_1565[] = {
  'o', 'm', 'i', 'd', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1565[] = {
  0x29b6
};
PRUnichar nsHtml5NamedCharacters::NAME_1566[] = {
  'o', 'm', 'i', 'n', 'u', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1566[] = {
  0x2296
};
PRUnichar nsHtml5NamedCharacters::NAME_1567[] = {
  'o', 'o', 'p', 'f', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1567[] = {
  0xd835, 0xdd60
};
PRUnichar nsHtml5NamedCharacters::NAME_1568[] = {
  'o', 'p', 'a', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1568[] = {
  0x29b7
};
PRUnichar nsHtml5NamedCharacters::NAME_1569[] = {
  'o', 'p', 'e', 'r', 'p', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1569[] = {
  0x29b9
};
PRUnichar nsHtml5NamedCharacters::NAME_1570[] = {
  'o', 'p', 'l', 'u', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1570[] = {
  0x2295
};
PRUnichar nsHtml5NamedCharacters::NAME_1571[] = {
  'o', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1571[] = {
  0x2228
};
PRUnichar nsHtml5NamedCharacters::NAME_1572[] = {
  'o', 'r', 'a', 'r', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1572[] = {
  0x21bb
};
PRUnichar nsHtml5NamedCharacters::NAME_1573[] = {
  'o', 'r', 'd', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1573[] = {
  0x2a5d
};
PRUnichar nsHtml5NamedCharacters::NAME_1574[] = {
  'o', 'r', 'd', 'e', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1574[] = {
  0x2134
};
PRUnichar nsHtml5NamedCharacters::NAME_1575[] = {
  'o', 'r', 'd', 'e', 'r', 'o', 'f', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1575[] = {
  0x2134
};
PRUnichar nsHtml5NamedCharacters::NAME_1576[] = {
  'o', 'r', 'd', 'f'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1576[] = {
  0x00aa
};
PRUnichar nsHtml5NamedCharacters::NAME_1577[] = {
  'o', 'r', 'd', 'f', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1577[] = {
  0x00aa
};
PRUnichar nsHtml5NamedCharacters::NAME_1578[] = {
  'o', 'r', 'd', 'm'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1578[] = {
  0x00ba
};
PRUnichar nsHtml5NamedCharacters::NAME_1579[] = {
  'o', 'r', 'd', 'm', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1579[] = {
  0x00ba
};
PRUnichar nsHtml5NamedCharacters::NAME_1580[] = {
  'o', 'r', 'i', 'g', 'o', 'f', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1580[] = {
  0x22b6
};
PRUnichar nsHtml5NamedCharacters::NAME_1581[] = {
  'o', 'r', 'o', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1581[] = {
  0x2a56
};
PRUnichar nsHtml5NamedCharacters::NAME_1582[] = {
  'o', 'r', 's', 'l', 'o', 'p', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1582[] = {
  0x2a57
};
PRUnichar nsHtml5NamedCharacters::NAME_1583[] = {
  'o', 'r', 'v', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1583[] = {
  0x2a5b
};
PRUnichar nsHtml5NamedCharacters::NAME_1584[] = {
  'o', 's', 'c', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1584[] = {
  0x2134
};
PRUnichar nsHtml5NamedCharacters::NAME_1585[] = {
  'o', 's', 'l', 'a', 's', 'h'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1585[] = {
  0x00f8
};
PRUnichar nsHtml5NamedCharacters::NAME_1586[] = {
  'o', 's', 'l', 'a', 's', 'h', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1586[] = {
  0x00f8
};
PRUnichar nsHtml5NamedCharacters::NAME_1587[] = {
  'o', 's', 'o', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1587[] = {
  0x2298
};
PRUnichar nsHtml5NamedCharacters::NAME_1588[] = {
  'o', 't', 'i', 'l', 'd', 'e'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1588[] = {
  0x00f5
};
PRUnichar nsHtml5NamedCharacters::NAME_1589[] = {
  'o', 't', 'i', 'l', 'd', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1589[] = {
  0x00f5
};
PRUnichar nsHtml5NamedCharacters::NAME_1590[] = {
  'o', 't', 'i', 'm', 'e', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1590[] = {
  0x2297
};
PRUnichar nsHtml5NamedCharacters::NAME_1591[] = {
  'o', 't', 'i', 'm', 'e', 's', 'a', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1591[] = {
  0x2a36
};
PRUnichar nsHtml5NamedCharacters::NAME_1592[] = {
  'o', 'u', 'm', 'l'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1592[] = {
  0x00f6
};
PRUnichar nsHtml5NamedCharacters::NAME_1593[] = {
  'o', 'u', 'm', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1593[] = {
  0x00f6
};
PRUnichar nsHtml5NamedCharacters::NAME_1594[] = {
  'o', 'v', 'b', 'a', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1594[] = {
  0x233d
};
PRUnichar nsHtml5NamedCharacters::NAME_1595[] = {
  'p', 'a', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1595[] = {
  0x2225
};
PRUnichar nsHtml5NamedCharacters::NAME_1596[] = {
  'p', 'a', 'r', 'a'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1596[] = {
  0x00b6
};
PRUnichar nsHtml5NamedCharacters::NAME_1597[] = {
  'p', 'a', 'r', 'a', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1597[] = {
  0x00b6
};
PRUnichar nsHtml5NamedCharacters::NAME_1598[] = {
  'p', 'a', 'r', 'a', 'l', 'l', 'e', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1598[] = {
  0x2225
};
PRUnichar nsHtml5NamedCharacters::NAME_1599[] = {
  'p', 'a', 'r', 's', 'i', 'm', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1599[] = {
  0x2af3
};
PRUnichar nsHtml5NamedCharacters::NAME_1600[] = {
  'p', 'a', 'r', 's', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1600[] = {
  0x2afd
};
PRUnichar nsHtml5NamedCharacters::NAME_1601[] = {
  'p', 'a', 'r', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1601[] = {
  0x2202
};
PRUnichar nsHtml5NamedCharacters::NAME_1602[] = {
  'p', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1602[] = {
  0x043f
};
PRUnichar nsHtml5NamedCharacters::NAME_1603[] = {
  'p', 'e', 'r', 'c', 'n', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1603[] = {
  0x0025
};
PRUnichar nsHtml5NamedCharacters::NAME_1604[] = {
  'p', 'e', 'r', 'i', 'o', 'd', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1604[] = {
  0x002e
};
PRUnichar nsHtml5NamedCharacters::NAME_1605[] = {
  'p', 'e', 'r', 'm', 'i', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1605[] = {
  0x2030
};
PRUnichar nsHtml5NamedCharacters::NAME_1606[] = {
  'p', 'e', 'r', 'p', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1606[] = {
  0x22a5
};
PRUnichar nsHtml5NamedCharacters::NAME_1607[] = {
  'p', 'e', 'r', 't', 'e', 'n', 'k', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1607[] = {
  0x2031
};
PRUnichar nsHtml5NamedCharacters::NAME_1608[] = {
  'p', 'f', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1608[] = {
  0xd835, 0xdd2d
};
PRUnichar nsHtml5NamedCharacters::NAME_1609[] = {
  'p', 'h', 'i', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1609[] = {
  0x03c6
};
PRUnichar nsHtml5NamedCharacters::NAME_1610[] = {
  'p', 'h', 'i', 'v', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1610[] = {
  0x03c6
};
PRUnichar nsHtml5NamedCharacters::NAME_1611[] = {
  'p', 'h', 'm', 'm', 'a', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1611[] = {
  0x2133
};
PRUnichar nsHtml5NamedCharacters::NAME_1612[] = {
  'p', 'h', 'o', 'n', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1612[] = {
  0x260e
};
PRUnichar nsHtml5NamedCharacters::NAME_1613[] = {
  'p', 'i', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1613[] = {
  0x03c0
};
PRUnichar nsHtml5NamedCharacters::NAME_1614[] = {
  'p', 'i', 't', 'c', 'h', 'f', 'o', 'r', 'k', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1614[] = {
  0x22d4
};
PRUnichar nsHtml5NamedCharacters::NAME_1615[] = {
  'p', 'i', 'v', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1615[] = {
  0x03d6
};
PRUnichar nsHtml5NamedCharacters::NAME_1616[] = {
  'p', 'l', 'a', 'n', 'c', 'k', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1616[] = {
  0x210f
};
PRUnichar nsHtml5NamedCharacters::NAME_1617[] = {
  'p', 'l', 'a', 'n', 'c', 'k', 'h', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1617[] = {
  0x210e
};
PRUnichar nsHtml5NamedCharacters::NAME_1618[] = {
  'p', 'l', 'a', 'n', 'k', 'v', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1618[] = {
  0x210f
};
PRUnichar nsHtml5NamedCharacters::NAME_1619[] = {
  'p', 'l', 'u', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1619[] = {
  0x002b
};
PRUnichar nsHtml5NamedCharacters::NAME_1620[] = {
  'p', 'l', 'u', 's', 'a', 'c', 'i', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1620[] = {
  0x2a23
};
PRUnichar nsHtml5NamedCharacters::NAME_1621[] = {
  'p', 'l', 'u', 's', 'b', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1621[] = {
  0x229e
};
PRUnichar nsHtml5NamedCharacters::NAME_1622[] = {
  'p', 'l', 'u', 's', 'c', 'i', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1622[] = {
  0x2a22
};
PRUnichar nsHtml5NamedCharacters::NAME_1623[] = {
  'p', 'l', 'u', 's', 'd', 'o', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1623[] = {
  0x2214
};
PRUnichar nsHtml5NamedCharacters::NAME_1624[] = {
  'p', 'l', 'u', 's', 'd', 'u', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1624[] = {
  0x2a25
};
PRUnichar nsHtml5NamedCharacters::NAME_1625[] = {
  'p', 'l', 'u', 's', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1625[] = {
  0x2a72
};
PRUnichar nsHtml5NamedCharacters::NAME_1626[] = {
  'p', 'l', 'u', 's', 'm', 'n'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1626[] = {
  0x00b1
};
PRUnichar nsHtml5NamedCharacters::NAME_1627[] = {
  'p', 'l', 'u', 's', 'm', 'n', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1627[] = {
  0x00b1
};
PRUnichar nsHtml5NamedCharacters::NAME_1628[] = {
  'p', 'l', 'u', 's', 's', 'i', 'm', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1628[] = {
  0x2a26
};
PRUnichar nsHtml5NamedCharacters::NAME_1629[] = {
  'p', 'l', 'u', 's', 't', 'w', 'o', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1629[] = {
  0x2a27
};
PRUnichar nsHtml5NamedCharacters::NAME_1630[] = {
  'p', 'm', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1630[] = {
  0x00b1
};
PRUnichar nsHtml5NamedCharacters::NAME_1631[] = {
  'p', 'o', 'i', 'n', 't', 'i', 'n', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1631[] = {
  0x2a15
};
PRUnichar nsHtml5NamedCharacters::NAME_1632[] = {
  'p', 'o', 'p', 'f', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1632[] = {
  0xd835, 0xdd61
};
PRUnichar nsHtml5NamedCharacters::NAME_1633[] = {
  'p', 'o', 'u', 'n', 'd'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1633[] = {
  0x00a3
};
PRUnichar nsHtml5NamedCharacters::NAME_1634[] = {
  'p', 'o', 'u', 'n', 'd', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1634[] = {
  0x00a3
};
PRUnichar nsHtml5NamedCharacters::NAME_1635[] = {
  'p', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1635[] = {
  0x227a
};
PRUnichar nsHtml5NamedCharacters::NAME_1636[] = {
  'p', 'r', 'E', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1636[] = {
  0x2ab3
};
PRUnichar nsHtml5NamedCharacters::NAME_1637[] = {
  'p', 'r', 'a', 'p', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1637[] = {
  0x2ab7
};
PRUnichar nsHtml5NamedCharacters::NAME_1638[] = {
  'p', 'r', 'c', 'u', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1638[] = {
  0x227c
};
PRUnichar nsHtml5NamedCharacters::NAME_1639[] = {
  'p', 'r', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1639[] = {
  0x2aaf
};
PRUnichar nsHtml5NamedCharacters::NAME_1640[] = {
  'p', 'r', 'e', 'c', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1640[] = {
  0x227a
};
PRUnichar nsHtml5NamedCharacters::NAME_1641[] = {
  'p', 'r', 'e', 'c', 'a', 'p', 'p', 'r', 'o', 'x', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1641[] = {
  0x2ab7
};
PRUnichar nsHtml5NamedCharacters::NAME_1642[] = {
  'p', 'r', 'e', 'c', 'c', 'u', 'r', 'l', 'y', 'e', 'q', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1642[] = {
  0x227c
};
PRUnichar nsHtml5NamedCharacters::NAME_1643[] = {
  'p', 'r', 'e', 'c', 'e', 'q', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1643[] = {
  0x2aaf
};
PRUnichar nsHtml5NamedCharacters::NAME_1644[] = {
  'p', 'r', 'e', 'c', 'n', 'a', 'p', 'p', 'r', 'o', 'x', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1644[] = {
  0x2ab9
};
PRUnichar nsHtml5NamedCharacters::NAME_1645[] = {
  'p', 'r', 'e', 'c', 'n', 'e', 'q', 'q', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1645[] = {
  0x2ab5
};
PRUnichar nsHtml5NamedCharacters::NAME_1646[] = {
  'p', 'r', 'e', 'c', 'n', 's', 'i', 'm', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1646[] = {
  0x22e8
};
PRUnichar nsHtml5NamedCharacters::NAME_1647[] = {
  'p', 'r', 'e', 'c', 's', 'i', 'm', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1647[] = {
  0x227e
};
PRUnichar nsHtml5NamedCharacters::NAME_1648[] = {
  'p', 'r', 'i', 'm', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1648[] = {
  0x2032
};
PRUnichar nsHtml5NamedCharacters::NAME_1649[] = {
  'p', 'r', 'i', 'm', 'e', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1649[] = {
  0x2119
};
PRUnichar nsHtml5NamedCharacters::NAME_1650[] = {
  'p', 'r', 'n', 'E', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1650[] = {
  0x2ab5
};
PRUnichar nsHtml5NamedCharacters::NAME_1651[] = {
  'p', 'r', 'n', 'a', 'p', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1651[] = {
  0x2ab9
};
PRUnichar nsHtml5NamedCharacters::NAME_1652[] = {
  'p', 'r', 'n', 's', 'i', 'm', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1652[] = {
  0x22e8
};
PRUnichar nsHtml5NamedCharacters::NAME_1653[] = {
  'p', 'r', 'o', 'd', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1653[] = {
  0x220f
};
PRUnichar nsHtml5NamedCharacters::NAME_1654[] = {
  'p', 'r', 'o', 'f', 'a', 'l', 'a', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1654[] = {
  0x232e
};
PRUnichar nsHtml5NamedCharacters::NAME_1655[] = {
  'p', 'r', 'o', 'f', 'l', 'i', 'n', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1655[] = {
  0x2312
};
PRUnichar nsHtml5NamedCharacters::NAME_1656[] = {
  'p', 'r', 'o', 'f', 's', 'u', 'r', 'f', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1656[] = {
  0x2313
};
PRUnichar nsHtml5NamedCharacters::NAME_1657[] = {
  'p', 'r', 'o', 'p', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1657[] = {
  0x221d
};
PRUnichar nsHtml5NamedCharacters::NAME_1658[] = {
  'p', 'r', 'o', 'p', 't', 'o', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1658[] = {
  0x221d
};
PRUnichar nsHtml5NamedCharacters::NAME_1659[] = {
  'p', 'r', 's', 'i', 'm', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1659[] = {
  0x227e
};
PRUnichar nsHtml5NamedCharacters::NAME_1660[] = {
  'p', 'r', 'u', 'r', 'e', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1660[] = {
  0x22b0
};
PRUnichar nsHtml5NamedCharacters::NAME_1661[] = {
  'p', 's', 'c', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1661[] = {
  0xd835, 0xdcc5
};
PRUnichar nsHtml5NamedCharacters::NAME_1662[] = {
  'p', 's', 'i', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1662[] = {
  0x03c8
};
PRUnichar nsHtml5NamedCharacters::NAME_1663[] = {
  'p', 'u', 'n', 'c', 's', 'p', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1663[] = {
  0x2008
};
PRUnichar nsHtml5NamedCharacters::NAME_1664[] = {
  'q', 'f', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1664[] = {
  0xd835, 0xdd2e
};
PRUnichar nsHtml5NamedCharacters::NAME_1665[] = {
  'q', 'i', 'n', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1665[] = {
  0x2a0c
};
PRUnichar nsHtml5NamedCharacters::NAME_1666[] = {
  'q', 'o', 'p', 'f', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1666[] = {
  0xd835, 0xdd62
};
PRUnichar nsHtml5NamedCharacters::NAME_1667[] = {
  'q', 'p', 'r', 'i', 'm', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1667[] = {
  0x2057
};
PRUnichar nsHtml5NamedCharacters::NAME_1668[] = {
  'q', 's', 'c', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1668[] = {
  0xd835, 0xdcc6
};
PRUnichar nsHtml5NamedCharacters::NAME_1669[] = {
  'q', 'u', 'a', 't', 'e', 'r', 'n', 'i', 'o', 'n', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1669[] = {
  0x210d
};
PRUnichar nsHtml5NamedCharacters::NAME_1670[] = {
  'q', 'u', 'a', 't', 'i', 'n', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1670[] = {
  0x2a16
};
PRUnichar nsHtml5NamedCharacters::NAME_1671[] = {
  'q', 'u', 'e', 's', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1671[] = {
  0x003f
};
PRUnichar nsHtml5NamedCharacters::NAME_1672[] = {
  'q', 'u', 'e', 's', 't', 'e', 'q', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1672[] = {
  0x225f
};
PRUnichar nsHtml5NamedCharacters::NAME_1673[] = {
  'q', 'u', 'o', 't'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1673[] = {
  0x0022
};
PRUnichar nsHtml5NamedCharacters::NAME_1674[] = {
  'q', 'u', 'o', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1674[] = {
  0x0022
};
PRUnichar nsHtml5NamedCharacters::NAME_1675[] = {
  'r', 'A', 'a', 'r', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1675[] = {
  0x21db
};
PRUnichar nsHtml5NamedCharacters::NAME_1676[] = {
  'r', 'A', 'r', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1676[] = {
  0x21d2
};
PRUnichar nsHtml5NamedCharacters::NAME_1677[] = {
  'r', 'A', 't', 'a', 'i', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1677[] = {
  0x291c
};
PRUnichar nsHtml5NamedCharacters::NAME_1678[] = {
  'r', 'B', 'a', 'r', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1678[] = {
  0x290f
};
PRUnichar nsHtml5NamedCharacters::NAME_1679[] = {
  'r', 'H', 'a', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1679[] = {
  0x2964
};
PRUnichar nsHtml5NamedCharacters::NAME_1680[] = {
  'r', 'a', 'c', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1680[] = {
  0x29da
};
PRUnichar nsHtml5NamedCharacters::NAME_1681[] = {
  'r', 'a', 'c', 'u', 't', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1681[] = {
  0x0155
};
PRUnichar nsHtml5NamedCharacters::NAME_1682[] = {
  'r', 'a', 'd', 'i', 'c', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1682[] = {
  0x221a
};
PRUnichar nsHtml5NamedCharacters::NAME_1683[] = {
  'r', 'a', 'e', 'm', 'p', 't', 'y', 'v', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1683[] = {
  0x29b3
};
PRUnichar nsHtml5NamedCharacters::NAME_1684[] = {
  'r', 'a', 'n', 'g', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1684[] = {
  0x27e9
};
PRUnichar nsHtml5NamedCharacters::NAME_1685[] = {
  'r', 'a', 'n', 'g', 'd', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1685[] = {
  0x2992
};
PRUnichar nsHtml5NamedCharacters::NAME_1686[] = {
  'r', 'a', 'n', 'g', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1686[] = {
  0x29a5
};
PRUnichar nsHtml5NamedCharacters::NAME_1687[] = {
  'r', 'a', 'n', 'g', 'l', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1687[] = {
  0x27e9
};
PRUnichar nsHtml5NamedCharacters::NAME_1688[] = {
  'r', 'a', 'q', 'u', 'o'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1688[] = {
  0x00bb
};
PRUnichar nsHtml5NamedCharacters::NAME_1689[] = {
  'r', 'a', 'q', 'u', 'o', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1689[] = {
  0x00bb
};
PRUnichar nsHtml5NamedCharacters::NAME_1690[] = {
  'r', 'a', 'r', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1690[] = {
  0x2192
};
PRUnichar nsHtml5NamedCharacters::NAME_1691[] = {
  'r', 'a', 'r', 'r', 'a', 'p', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1691[] = {
  0x2975
};
PRUnichar nsHtml5NamedCharacters::NAME_1692[] = {
  'r', 'a', 'r', 'r', 'b', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1692[] = {
  0x21e5
};
PRUnichar nsHtml5NamedCharacters::NAME_1693[] = {
  'r', 'a', 'r', 'r', 'b', 'f', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1693[] = {
  0x2920
};
PRUnichar nsHtml5NamedCharacters::NAME_1694[] = {
  'r', 'a', 'r', 'r', 'c', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1694[] = {
  0x2933
};
PRUnichar nsHtml5NamedCharacters::NAME_1695[] = {
  'r', 'a', 'r', 'r', 'f', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1695[] = {
  0x291e
};
PRUnichar nsHtml5NamedCharacters::NAME_1696[] = {
  'r', 'a', 'r', 'r', 'h', 'k', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1696[] = {
  0x21aa
};
PRUnichar nsHtml5NamedCharacters::NAME_1697[] = {
  'r', 'a', 'r', 'r', 'l', 'p', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1697[] = {
  0x21ac
};
PRUnichar nsHtml5NamedCharacters::NAME_1698[] = {
  'r', 'a', 'r', 'r', 'p', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1698[] = {
  0x2945
};
PRUnichar nsHtml5NamedCharacters::NAME_1699[] = {
  'r', 'a', 'r', 'r', 's', 'i', 'm', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1699[] = {
  0x2974
};
PRUnichar nsHtml5NamedCharacters::NAME_1700[] = {
  'r', 'a', 'r', 'r', 't', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1700[] = {
  0x21a3
};
PRUnichar nsHtml5NamedCharacters::NAME_1701[] = {
  'r', 'a', 'r', 'r', 'w', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1701[] = {
  0x219d
};
PRUnichar nsHtml5NamedCharacters::NAME_1702[] = {
  'r', 'a', 't', 'a', 'i', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1702[] = {
  0x291a
};
PRUnichar nsHtml5NamedCharacters::NAME_1703[] = {
  'r', 'a', 't', 'i', 'o', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1703[] = {
  0x2236
};
PRUnichar nsHtml5NamedCharacters::NAME_1704[] = {
  'r', 'a', 't', 'i', 'o', 'n', 'a', 'l', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1704[] = {
  0x211a
};
PRUnichar nsHtml5NamedCharacters::NAME_1705[] = {
  'r', 'b', 'a', 'r', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1705[] = {
  0x290d
};
PRUnichar nsHtml5NamedCharacters::NAME_1706[] = {
  'r', 'b', 'b', 'r', 'k', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1706[] = {
  0x2773
};
PRUnichar nsHtml5NamedCharacters::NAME_1707[] = {
  'r', 'b', 'r', 'a', 'c', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1707[] = {
  0x007d
};
PRUnichar nsHtml5NamedCharacters::NAME_1708[] = {
  'r', 'b', 'r', 'a', 'c', 'k', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1708[] = {
  0x005d
};
PRUnichar nsHtml5NamedCharacters::NAME_1709[] = {
  'r', 'b', 'r', 'k', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1709[] = {
  0x298c
};
PRUnichar nsHtml5NamedCharacters::NAME_1710[] = {
  'r', 'b', 'r', 'k', 's', 'l', 'd', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1710[] = {
  0x298e
};
PRUnichar nsHtml5NamedCharacters::NAME_1711[] = {
  'r', 'b', 'r', 'k', 's', 'l', 'u', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1711[] = {
  0x2990
};
PRUnichar nsHtml5NamedCharacters::NAME_1712[] = {
  'r', 'c', 'a', 'r', 'o', 'n', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1712[] = {
  0x0159
};
PRUnichar nsHtml5NamedCharacters::NAME_1713[] = {
  'r', 'c', 'e', 'd', 'i', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1713[] = {
  0x0157
};
PRUnichar nsHtml5NamedCharacters::NAME_1714[] = {
  'r', 'c', 'e', 'i', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1714[] = {
  0x2309
};
PRUnichar nsHtml5NamedCharacters::NAME_1715[] = {
  'r', 'c', 'u', 'b', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1715[] = {
  0x007d
};
PRUnichar nsHtml5NamedCharacters::NAME_1716[] = {
  'r', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1716[] = {
  0x0440
};
PRUnichar nsHtml5NamedCharacters::NAME_1717[] = {
  'r', 'd', 'c', 'a', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1717[] = {
  0x2937
};
PRUnichar nsHtml5NamedCharacters::NAME_1718[] = {
  'r', 'd', 'l', 'd', 'h', 'a', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1718[] = {
  0x2969
};
PRUnichar nsHtml5NamedCharacters::NAME_1719[] = {
  'r', 'd', 'q', 'u', 'o', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1719[] = {
  0x201d
};
PRUnichar nsHtml5NamedCharacters::NAME_1720[] = {
  'r', 'd', 'q', 'u', 'o', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1720[] = {
  0x201d
};
PRUnichar nsHtml5NamedCharacters::NAME_1721[] = {
  'r', 'd', 's', 'h', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1721[] = {
  0x21b3
};
PRUnichar nsHtml5NamedCharacters::NAME_1722[] = {
  'r', 'e', 'a', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1722[] = {
  0x211c
};
PRUnichar nsHtml5NamedCharacters::NAME_1723[] = {
  'r', 'e', 'a', 'l', 'i', 'n', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1723[] = {
  0x211b
};
PRUnichar nsHtml5NamedCharacters::NAME_1724[] = {
  'r', 'e', 'a', 'l', 'p', 'a', 'r', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1724[] = {
  0x211c
};
PRUnichar nsHtml5NamedCharacters::NAME_1725[] = {
  'r', 'e', 'a', 'l', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1725[] = {
  0x211d
};
PRUnichar nsHtml5NamedCharacters::NAME_1726[] = {
  'r', 'e', 'c', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1726[] = {
  0x25ad
};
PRUnichar nsHtml5NamedCharacters::NAME_1727[] = {
  'r', 'e', 'g'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1727[] = {
  0x00ae
};
PRUnichar nsHtml5NamedCharacters::NAME_1728[] = {
  'r', 'e', 'g', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1728[] = {
  0x00ae
};
PRUnichar nsHtml5NamedCharacters::NAME_1729[] = {
  'r', 'f', 'i', 's', 'h', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1729[] = {
  0x297d
};
PRUnichar nsHtml5NamedCharacters::NAME_1730[] = {
  'r', 'f', 'l', 'o', 'o', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1730[] = {
  0x230b
};
PRUnichar nsHtml5NamedCharacters::NAME_1731[] = {
  'r', 'f', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1731[] = {
  0xd835, 0xdd2f
};
PRUnichar nsHtml5NamedCharacters::NAME_1732[] = {
  'r', 'h', 'a', 'r', 'd', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1732[] = {
  0x21c1
};
PRUnichar nsHtml5NamedCharacters::NAME_1733[] = {
  'r', 'h', 'a', 'r', 'u', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1733[] = {
  0x21c0
};
PRUnichar nsHtml5NamedCharacters::NAME_1734[] = {
  'r', 'h', 'a', 'r', 'u', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1734[] = {
  0x296c
};
PRUnichar nsHtml5NamedCharacters::NAME_1735[] = {
  'r', 'h', 'o', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1735[] = {
  0x03c1
};
PRUnichar nsHtml5NamedCharacters::NAME_1736[] = {
  'r', 'h', 'o', 'v', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1736[] = {
  0x03f1
};
PRUnichar nsHtml5NamedCharacters::NAME_1737[] = {
  'r', 'i', 'g', 'h', 't', 'a', 'r', 'r', 'o', 'w', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1737[] = {
  0x2192
};
PRUnichar nsHtml5NamedCharacters::NAME_1738[] = {
  'r', 'i', 'g', 'h', 't', 'a', 'r', 'r', 'o', 'w', 't', 'a', 'i', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1738[] = {
  0x21a3
};
PRUnichar nsHtml5NamedCharacters::NAME_1739[] = {
  'r', 'i', 'g', 'h', 't', 'h', 'a', 'r', 'p', 'o', 'o', 'n', 'd', 'o', 'w', 'n', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1739[] = {
  0x21c1
};
PRUnichar nsHtml5NamedCharacters::NAME_1740[] = {
  'r', 'i', 'g', 'h', 't', 'h', 'a', 'r', 'p', 'o', 'o', 'n', 'u', 'p', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1740[] = {
  0x21c0
};
PRUnichar nsHtml5NamedCharacters::NAME_1741[] = {
  'r', 'i', 'g', 'h', 't', 'l', 'e', 'f', 't', 'a', 'r', 'r', 'o', 'w', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1741[] = {
  0x21c4
};
PRUnichar nsHtml5NamedCharacters::NAME_1742[] = {
  'r', 'i', 'g', 'h', 't', 'l', 'e', 'f', 't', 'h', 'a', 'r', 'p', 'o', 'o', 'n', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1742[] = {
  0x21cc
};
PRUnichar nsHtml5NamedCharacters::NAME_1743[] = {
  'r', 'i', 'g', 'h', 't', 'r', 'i', 'g', 'h', 't', 'a', 'r', 'r', 'o', 'w', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1743[] = {
  0x21c9
};
PRUnichar nsHtml5NamedCharacters::NAME_1744[] = {
  'r', 'i', 'g', 'h', 't', 's', 'q', 'u', 'i', 'g', 'a', 'r', 'r', 'o', 'w', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1744[] = {
  0x219d
};
PRUnichar nsHtml5NamedCharacters::NAME_1745[] = {
  'r', 'i', 'g', 'h', 't', 't', 'h', 'r', 'e', 'e', 't', 'i', 'm', 'e', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1745[] = {
  0x22cc
};
PRUnichar nsHtml5NamedCharacters::NAME_1746[] = {
  'r', 'i', 'n', 'g', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1746[] = {
  0x02da
};
PRUnichar nsHtml5NamedCharacters::NAME_1747[] = {
  'r', 'i', 's', 'i', 'n', 'g', 'd', 'o', 't', 's', 'e', 'q', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1747[] = {
  0x2253
};
PRUnichar nsHtml5NamedCharacters::NAME_1748[] = {
  'r', 'l', 'a', 'r', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1748[] = {
  0x21c4
};
PRUnichar nsHtml5NamedCharacters::NAME_1749[] = {
  'r', 'l', 'h', 'a', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1749[] = {
  0x21cc
};
PRUnichar nsHtml5NamedCharacters::NAME_1750[] = {
  'r', 'l', 'm', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1750[] = {
  0x200f
};
PRUnichar nsHtml5NamedCharacters::NAME_1751[] = {
  'r', 'm', 'o', 'u', 's', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1751[] = {
  0x23b1
};
PRUnichar nsHtml5NamedCharacters::NAME_1752[] = {
  'r', 'm', 'o', 'u', 's', 't', 'a', 'c', 'h', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1752[] = {
  0x23b1
};
PRUnichar nsHtml5NamedCharacters::NAME_1753[] = {
  'r', 'n', 'm', 'i', 'd', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1753[] = {
  0x2aee
};
PRUnichar nsHtml5NamedCharacters::NAME_1754[] = {
  'r', 'o', 'a', 'n', 'g', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1754[] = {
  0x27ed
};
PRUnichar nsHtml5NamedCharacters::NAME_1755[] = {
  'r', 'o', 'a', 'r', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1755[] = {
  0x21fe
};
PRUnichar nsHtml5NamedCharacters::NAME_1756[] = {
  'r', 'o', 'b', 'r', 'k', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1756[] = {
  0x27e7
};
PRUnichar nsHtml5NamedCharacters::NAME_1757[] = {
  'r', 'o', 'p', 'a', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1757[] = {
  0x2986
};
PRUnichar nsHtml5NamedCharacters::NAME_1758[] = {
  'r', 'o', 'p', 'f', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1758[] = {
  0xd835, 0xdd63
};
PRUnichar nsHtml5NamedCharacters::NAME_1759[] = {
  'r', 'o', 'p', 'l', 'u', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1759[] = {
  0x2a2e
};
PRUnichar nsHtml5NamedCharacters::NAME_1760[] = {
  'r', 'o', 't', 'i', 'm', 'e', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1760[] = {
  0x2a35
};
PRUnichar nsHtml5NamedCharacters::NAME_1761[] = {
  'r', 'p', 'a', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1761[] = {
  0x0029
};
PRUnichar nsHtml5NamedCharacters::NAME_1762[] = {
  'r', 'p', 'a', 'r', 'g', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1762[] = {
  0x2994
};
PRUnichar nsHtml5NamedCharacters::NAME_1763[] = {
  'r', 'p', 'p', 'o', 'l', 'i', 'n', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1763[] = {
  0x2a12
};
PRUnichar nsHtml5NamedCharacters::NAME_1764[] = {
  'r', 'r', 'a', 'r', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1764[] = {
  0x21c9
};
PRUnichar nsHtml5NamedCharacters::NAME_1765[] = {
  'r', 's', 'a', 'q', 'u', 'o', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1765[] = {
  0x203a
};
PRUnichar nsHtml5NamedCharacters::NAME_1766[] = {
  'r', 's', 'c', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1766[] = {
  0xd835, 0xdcc7
};
PRUnichar nsHtml5NamedCharacters::NAME_1767[] = {
  'r', 's', 'h', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1767[] = {
  0x21b1
};
PRUnichar nsHtml5NamedCharacters::NAME_1768[] = {
  'r', 's', 'q', 'b', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1768[] = {
  0x005d
};
PRUnichar nsHtml5NamedCharacters::NAME_1769[] = {
  'r', 's', 'q', 'u', 'o', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1769[] = {
  0x2019
};
PRUnichar nsHtml5NamedCharacters::NAME_1770[] = {
  'r', 's', 'q', 'u', 'o', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1770[] = {
  0x2019
};
PRUnichar nsHtml5NamedCharacters::NAME_1771[] = {
  'r', 't', 'h', 'r', 'e', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1771[] = {
  0x22cc
};
PRUnichar nsHtml5NamedCharacters::NAME_1772[] = {
  'r', 't', 'i', 'm', 'e', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1772[] = {
  0x22ca
};
PRUnichar nsHtml5NamedCharacters::NAME_1773[] = {
  'r', 't', 'r', 'i', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1773[] = {
  0x25b9
};
PRUnichar nsHtml5NamedCharacters::NAME_1774[] = {
  'r', 't', 'r', 'i', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1774[] = {
  0x22b5
};
PRUnichar nsHtml5NamedCharacters::NAME_1775[] = {
  'r', 't', 'r', 'i', 'f', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1775[] = {
  0x25b8
};
PRUnichar nsHtml5NamedCharacters::NAME_1776[] = {
  'r', 't', 'r', 'i', 'l', 't', 'r', 'i', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1776[] = {
  0x29ce
};
PRUnichar nsHtml5NamedCharacters::NAME_1777[] = {
  'r', 'u', 'l', 'u', 'h', 'a', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1777[] = {
  0x2968
};
PRUnichar nsHtml5NamedCharacters::NAME_1778[] = {
  'r', 'x', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1778[] = {
  0x211e
};
PRUnichar nsHtml5NamedCharacters::NAME_1779[] = {
  's', 'a', 'c', 'u', 't', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1779[] = {
  0x015b
};
PRUnichar nsHtml5NamedCharacters::NAME_1780[] = {
  's', 'b', 'q', 'u', 'o', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1780[] = {
  0x201a
};
PRUnichar nsHtml5NamedCharacters::NAME_1781[] = {
  's', 'c', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1781[] = {
  0x227b
};
PRUnichar nsHtml5NamedCharacters::NAME_1782[] = {
  's', 'c', 'E', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1782[] = {
  0x2ab4
};
PRUnichar nsHtml5NamedCharacters::NAME_1783[] = {
  's', 'c', 'a', 'p', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1783[] = {
  0x2ab8
};
PRUnichar nsHtml5NamedCharacters::NAME_1784[] = {
  's', 'c', 'a', 'r', 'o', 'n', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1784[] = {
  0x0161
};
PRUnichar nsHtml5NamedCharacters::NAME_1785[] = {
  's', 'c', 'c', 'u', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1785[] = {
  0x227d
};
PRUnichar nsHtml5NamedCharacters::NAME_1786[] = {
  's', 'c', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1786[] = {
  0x2ab0
};
PRUnichar nsHtml5NamedCharacters::NAME_1787[] = {
  's', 'c', 'e', 'd', 'i', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1787[] = {
  0x015f
};
PRUnichar nsHtml5NamedCharacters::NAME_1788[] = {
  's', 'c', 'i', 'r', 'c', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1788[] = {
  0x015d
};
PRUnichar nsHtml5NamedCharacters::NAME_1789[] = {
  's', 'c', 'n', 'E', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1789[] = {
  0x2ab6
};
PRUnichar nsHtml5NamedCharacters::NAME_1790[] = {
  's', 'c', 'n', 'a', 'p', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1790[] = {
  0x2aba
};
PRUnichar nsHtml5NamedCharacters::NAME_1791[] = {
  's', 'c', 'n', 's', 'i', 'm', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1791[] = {
  0x22e9
};
PRUnichar nsHtml5NamedCharacters::NAME_1792[] = {
  's', 'c', 'p', 'o', 'l', 'i', 'n', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1792[] = {
  0x2a13
};
PRUnichar nsHtml5NamedCharacters::NAME_1793[] = {
  's', 'c', 's', 'i', 'm', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1793[] = {
  0x227f
};
PRUnichar nsHtml5NamedCharacters::NAME_1794[] = {
  's', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1794[] = {
  0x0441
};
PRUnichar nsHtml5NamedCharacters::NAME_1795[] = {
  's', 'd', 'o', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1795[] = {
  0x22c5
};
PRUnichar nsHtml5NamedCharacters::NAME_1796[] = {
  's', 'd', 'o', 't', 'b', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1796[] = {
  0x22a1
};
PRUnichar nsHtml5NamedCharacters::NAME_1797[] = {
  's', 'd', 'o', 't', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1797[] = {
  0x2a66
};
PRUnichar nsHtml5NamedCharacters::NAME_1798[] = {
  's', 'e', 'A', 'r', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1798[] = {
  0x21d8
};
PRUnichar nsHtml5NamedCharacters::NAME_1799[] = {
  's', 'e', 'a', 'r', 'h', 'k', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1799[] = {
  0x2925
};
PRUnichar nsHtml5NamedCharacters::NAME_1800[] = {
  's', 'e', 'a', 'r', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1800[] = {
  0x2198
};
PRUnichar nsHtml5NamedCharacters::NAME_1801[] = {
  's', 'e', 'a', 'r', 'r', 'o', 'w', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1801[] = {
  0x2198
};
PRUnichar nsHtml5NamedCharacters::NAME_1802[] = {
  's', 'e', 'c', 't'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1802[] = {
  0x00a7
};
PRUnichar nsHtml5NamedCharacters::NAME_1803[] = {
  's', 'e', 'c', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1803[] = {
  0x00a7
};
PRUnichar nsHtml5NamedCharacters::NAME_1804[] = {
  's', 'e', 'm', 'i', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1804[] = {
  0x003b
};
PRUnichar nsHtml5NamedCharacters::NAME_1805[] = {
  's', 'e', 's', 'w', 'a', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1805[] = {
  0x2929
};
PRUnichar nsHtml5NamedCharacters::NAME_1806[] = {
  's', 'e', 't', 'm', 'i', 'n', 'u', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1806[] = {
  0x2216
};
PRUnichar nsHtml5NamedCharacters::NAME_1807[] = {
  's', 'e', 't', 'm', 'n', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1807[] = {
  0x2216
};
PRUnichar nsHtml5NamedCharacters::NAME_1808[] = {
  's', 'e', 'x', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1808[] = {
  0x2736
};
PRUnichar nsHtml5NamedCharacters::NAME_1809[] = {
  's', 'f', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1809[] = {
  0xd835, 0xdd30
};
PRUnichar nsHtml5NamedCharacters::NAME_1810[] = {
  's', 'f', 'r', 'o', 'w', 'n', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1810[] = {
  0x2322
};
PRUnichar nsHtml5NamedCharacters::NAME_1811[] = {
  's', 'h', 'a', 'r', 'p', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1811[] = {
  0x266f
};
PRUnichar nsHtml5NamedCharacters::NAME_1812[] = {
  's', 'h', 'c', 'h', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1812[] = {
  0x0449
};
PRUnichar nsHtml5NamedCharacters::NAME_1813[] = {
  's', 'h', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1813[] = {
  0x0448
};
PRUnichar nsHtml5NamedCharacters::NAME_1814[] = {
  's', 'h', 'o', 'r', 't', 'm', 'i', 'd', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1814[] = {
  0x2223
};
PRUnichar nsHtml5NamedCharacters::NAME_1815[] = {
  's', 'h', 'o', 'r', 't', 'p', 'a', 'r', 'a', 'l', 'l', 'e', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1815[] = {
  0x2225
};
PRUnichar nsHtml5NamedCharacters::NAME_1816[] = {
  's', 'h', 'y'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1816[] = {
  0x00ad
};
PRUnichar nsHtml5NamedCharacters::NAME_1817[] = {
  's', 'h', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1817[] = {
  0x00ad
};
PRUnichar nsHtml5NamedCharacters::NAME_1818[] = {
  's', 'i', 'g', 'm', 'a', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1818[] = {
  0x03c3
};
PRUnichar nsHtml5NamedCharacters::NAME_1819[] = {
  's', 'i', 'g', 'm', 'a', 'f', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1819[] = {
  0x03c2
};
PRUnichar nsHtml5NamedCharacters::NAME_1820[] = {
  's', 'i', 'g', 'm', 'a', 'v', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1820[] = {
  0x03c2
};
PRUnichar nsHtml5NamedCharacters::NAME_1821[] = {
  's', 'i', 'm', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1821[] = {
  0x223c
};
PRUnichar nsHtml5NamedCharacters::NAME_1822[] = {
  's', 'i', 'm', 'd', 'o', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1822[] = {
  0x2a6a
};
PRUnichar nsHtml5NamedCharacters::NAME_1823[] = {
  's', 'i', 'm', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1823[] = {
  0x2243
};
PRUnichar nsHtml5NamedCharacters::NAME_1824[] = {
  's', 'i', 'm', 'e', 'q', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1824[] = {
  0x2243
};
PRUnichar nsHtml5NamedCharacters::NAME_1825[] = {
  's', 'i', 'm', 'g', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1825[] = {
  0x2a9e
};
PRUnichar nsHtml5NamedCharacters::NAME_1826[] = {
  's', 'i', 'm', 'g', 'E', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1826[] = {
  0x2aa0
};
PRUnichar nsHtml5NamedCharacters::NAME_1827[] = {
  's', 'i', 'm', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1827[] = {
  0x2a9d
};
PRUnichar nsHtml5NamedCharacters::NAME_1828[] = {
  's', 'i', 'm', 'l', 'E', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1828[] = {
  0x2a9f
};
PRUnichar nsHtml5NamedCharacters::NAME_1829[] = {
  's', 'i', 'm', 'n', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1829[] = {
  0x2246
};
PRUnichar nsHtml5NamedCharacters::NAME_1830[] = {
  's', 'i', 'm', 'p', 'l', 'u', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1830[] = {
  0x2a24
};
PRUnichar nsHtml5NamedCharacters::NAME_1831[] = {
  's', 'i', 'm', 'r', 'a', 'r', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1831[] = {
  0x2972
};
PRUnichar nsHtml5NamedCharacters::NAME_1832[] = {
  's', 'l', 'a', 'r', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1832[] = {
  0x2190
};
PRUnichar nsHtml5NamedCharacters::NAME_1833[] = {
  's', 'm', 'a', 'l', 'l', 's', 'e', 't', 'm', 'i', 'n', 'u', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1833[] = {
  0x2216
};
PRUnichar nsHtml5NamedCharacters::NAME_1834[] = {
  's', 'm', 'a', 's', 'h', 'p', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1834[] = {
  0x2a33
};
PRUnichar nsHtml5NamedCharacters::NAME_1835[] = {
  's', 'm', 'e', 'p', 'a', 'r', 's', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1835[] = {
  0x29e4
};
PRUnichar nsHtml5NamedCharacters::NAME_1836[] = {
  's', 'm', 'i', 'd', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1836[] = {
  0x2223
};
PRUnichar nsHtml5NamedCharacters::NAME_1837[] = {
  's', 'm', 'i', 'l', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1837[] = {
  0x2323
};
PRUnichar nsHtml5NamedCharacters::NAME_1838[] = {
  's', 'm', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1838[] = {
  0x2aaa
};
PRUnichar nsHtml5NamedCharacters::NAME_1839[] = {
  's', 'm', 't', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1839[] = {
  0x2aac
};
PRUnichar nsHtml5NamedCharacters::NAME_1840[] = {
  's', 'o', 'f', 't', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1840[] = {
  0x044c
};
PRUnichar nsHtml5NamedCharacters::NAME_1841[] = {
  's', 'o', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1841[] = {
  0x002f
};
PRUnichar nsHtml5NamedCharacters::NAME_1842[] = {
  's', 'o', 'l', 'b', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1842[] = {
  0x29c4
};
PRUnichar nsHtml5NamedCharacters::NAME_1843[] = {
  's', 'o', 'l', 'b', 'a', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1843[] = {
  0x233f
};
PRUnichar nsHtml5NamedCharacters::NAME_1844[] = {
  's', 'o', 'p', 'f', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1844[] = {
  0xd835, 0xdd64
};
PRUnichar nsHtml5NamedCharacters::NAME_1845[] = {
  's', 'p', 'a', 'd', 'e', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1845[] = {
  0x2660
};
PRUnichar nsHtml5NamedCharacters::NAME_1846[] = {
  's', 'p', 'a', 'd', 'e', 's', 'u', 'i', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1846[] = {
  0x2660
};
PRUnichar nsHtml5NamedCharacters::NAME_1847[] = {
  's', 'p', 'a', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1847[] = {
  0x2225
};
PRUnichar nsHtml5NamedCharacters::NAME_1848[] = {
  's', 'q', 'c', 'a', 'p', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1848[] = {
  0x2293
};
PRUnichar nsHtml5NamedCharacters::NAME_1849[] = {
  's', 'q', 'c', 'u', 'p', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1849[] = {
  0x2294
};
PRUnichar nsHtml5NamedCharacters::NAME_1850[] = {
  's', 'q', 's', 'u', 'b', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1850[] = {
  0x228f
};
PRUnichar nsHtml5NamedCharacters::NAME_1851[] = {
  's', 'q', 's', 'u', 'b', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1851[] = {
  0x2291
};
PRUnichar nsHtml5NamedCharacters::NAME_1852[] = {
  's', 'q', 's', 'u', 'b', 's', 'e', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1852[] = {
  0x228f
};
PRUnichar nsHtml5NamedCharacters::NAME_1853[] = {
  's', 'q', 's', 'u', 'b', 's', 'e', 't', 'e', 'q', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1853[] = {
  0x2291
};
PRUnichar nsHtml5NamedCharacters::NAME_1854[] = {
  's', 'q', 's', 'u', 'p', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1854[] = {
  0x2290
};
PRUnichar nsHtml5NamedCharacters::NAME_1855[] = {
  's', 'q', 's', 'u', 'p', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1855[] = {
  0x2292
};
PRUnichar nsHtml5NamedCharacters::NAME_1856[] = {
  's', 'q', 's', 'u', 'p', 's', 'e', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1856[] = {
  0x2290
};
PRUnichar nsHtml5NamedCharacters::NAME_1857[] = {
  's', 'q', 's', 'u', 'p', 's', 'e', 't', 'e', 'q', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1857[] = {
  0x2292
};
PRUnichar nsHtml5NamedCharacters::NAME_1858[] = {
  's', 'q', 'u', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1858[] = {
  0x25a1
};
PRUnichar nsHtml5NamedCharacters::NAME_1859[] = {
  's', 'q', 'u', 'a', 'r', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1859[] = {
  0x25a1
};
PRUnichar nsHtml5NamedCharacters::NAME_1860[] = {
  's', 'q', 'u', 'a', 'r', 'f', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1860[] = {
  0x25aa
};
PRUnichar nsHtml5NamedCharacters::NAME_1861[] = {
  's', 'q', 'u', 'f', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1861[] = {
  0x25aa
};
PRUnichar nsHtml5NamedCharacters::NAME_1862[] = {
  's', 'r', 'a', 'r', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1862[] = {
  0x2192
};
PRUnichar nsHtml5NamedCharacters::NAME_1863[] = {
  's', 's', 'c', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1863[] = {
  0xd835, 0xdcc8
};
PRUnichar nsHtml5NamedCharacters::NAME_1864[] = {
  's', 's', 'e', 't', 'm', 'n', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1864[] = {
  0x2216
};
PRUnichar nsHtml5NamedCharacters::NAME_1865[] = {
  's', 's', 'm', 'i', 'l', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1865[] = {
  0x2323
};
PRUnichar nsHtml5NamedCharacters::NAME_1866[] = {
  's', 's', 't', 'a', 'r', 'f', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1866[] = {
  0x22c6
};
PRUnichar nsHtml5NamedCharacters::NAME_1867[] = {
  's', 't', 'a', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1867[] = {
  0x2606
};
PRUnichar nsHtml5NamedCharacters::NAME_1868[] = {
  's', 't', 'a', 'r', 'f', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1868[] = {
  0x2605
};
PRUnichar nsHtml5NamedCharacters::NAME_1869[] = {
  's', 't', 'r', 'a', 'i', 'g', 'h', 't', 'e', 'p', 's', 'i', 'l', 'o', 'n', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1869[] = {
  0x03f5
};
PRUnichar nsHtml5NamedCharacters::NAME_1870[] = {
  's', 't', 'r', 'a', 'i', 'g', 'h', 't', 'p', 'h', 'i', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1870[] = {
  0x03d5
};
PRUnichar nsHtml5NamedCharacters::NAME_1871[] = {
  's', 't', 'r', 'n', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1871[] = {
  0x00af
};
PRUnichar nsHtml5NamedCharacters::NAME_1872[] = {
  's', 'u', 'b', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1872[] = {
  0x2282
};
PRUnichar nsHtml5NamedCharacters::NAME_1873[] = {
  's', 'u', 'b', 'E', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1873[] = {
  0x2ac5
};
PRUnichar nsHtml5NamedCharacters::NAME_1874[] = {
  's', 'u', 'b', 'd', 'o', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1874[] = {
  0x2abd
};
PRUnichar nsHtml5NamedCharacters::NAME_1875[] = {
  's', 'u', 'b', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1875[] = {
  0x2286
};
PRUnichar nsHtml5NamedCharacters::NAME_1876[] = {
  's', 'u', 'b', 'e', 'd', 'o', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1876[] = {
  0x2ac3
};
PRUnichar nsHtml5NamedCharacters::NAME_1877[] = {
  's', 'u', 'b', 'm', 'u', 'l', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1877[] = {
  0x2ac1
};
PRUnichar nsHtml5NamedCharacters::NAME_1878[] = {
  's', 'u', 'b', 'n', 'E', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1878[] = {
  0x2acb
};
PRUnichar nsHtml5NamedCharacters::NAME_1879[] = {
  's', 'u', 'b', 'n', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1879[] = {
  0x228a
};
PRUnichar nsHtml5NamedCharacters::NAME_1880[] = {
  's', 'u', 'b', 'p', 'l', 'u', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1880[] = {
  0x2abf
};
PRUnichar nsHtml5NamedCharacters::NAME_1881[] = {
  's', 'u', 'b', 'r', 'a', 'r', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1881[] = {
  0x2979
};
PRUnichar nsHtml5NamedCharacters::NAME_1882[] = {
  's', 'u', 'b', 's', 'e', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1882[] = {
  0x2282
};
PRUnichar nsHtml5NamedCharacters::NAME_1883[] = {
  's', 'u', 'b', 's', 'e', 't', 'e', 'q', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1883[] = {
  0x2286
};
PRUnichar nsHtml5NamedCharacters::NAME_1884[] = {
  's', 'u', 'b', 's', 'e', 't', 'e', 'q', 'q', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1884[] = {
  0x2ac5
};
PRUnichar nsHtml5NamedCharacters::NAME_1885[] = {
  's', 'u', 'b', 's', 'e', 't', 'n', 'e', 'q', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1885[] = {
  0x228a
};
PRUnichar nsHtml5NamedCharacters::NAME_1886[] = {
  's', 'u', 'b', 's', 'e', 't', 'n', 'e', 'q', 'q', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1886[] = {
  0x2acb
};
PRUnichar nsHtml5NamedCharacters::NAME_1887[] = {
  's', 'u', 'b', 's', 'i', 'm', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1887[] = {
  0x2ac7
};
PRUnichar nsHtml5NamedCharacters::NAME_1888[] = {
  's', 'u', 'b', 's', 'u', 'b', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1888[] = {
  0x2ad5
};
PRUnichar nsHtml5NamedCharacters::NAME_1889[] = {
  's', 'u', 'b', 's', 'u', 'p', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1889[] = {
  0x2ad3
};
PRUnichar nsHtml5NamedCharacters::NAME_1890[] = {
  's', 'u', 'c', 'c', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1890[] = {
  0x227b
};
PRUnichar nsHtml5NamedCharacters::NAME_1891[] = {
  's', 'u', 'c', 'c', 'a', 'p', 'p', 'r', 'o', 'x', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1891[] = {
  0x2ab8
};
PRUnichar nsHtml5NamedCharacters::NAME_1892[] = {
  's', 'u', 'c', 'c', 'c', 'u', 'r', 'l', 'y', 'e', 'q', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1892[] = {
  0x227d
};
PRUnichar nsHtml5NamedCharacters::NAME_1893[] = {
  's', 'u', 'c', 'c', 'e', 'q', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1893[] = {
  0x2ab0
};
PRUnichar nsHtml5NamedCharacters::NAME_1894[] = {
  's', 'u', 'c', 'c', 'n', 'a', 'p', 'p', 'r', 'o', 'x', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1894[] = {
  0x2aba
};
PRUnichar nsHtml5NamedCharacters::NAME_1895[] = {
  's', 'u', 'c', 'c', 'n', 'e', 'q', 'q', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1895[] = {
  0x2ab6
};
PRUnichar nsHtml5NamedCharacters::NAME_1896[] = {
  's', 'u', 'c', 'c', 'n', 's', 'i', 'm', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1896[] = {
  0x22e9
};
PRUnichar nsHtml5NamedCharacters::NAME_1897[] = {
  's', 'u', 'c', 'c', 's', 'i', 'm', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1897[] = {
  0x227f
};
PRUnichar nsHtml5NamedCharacters::NAME_1898[] = {
  's', 'u', 'm', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1898[] = {
  0x2211
};
PRUnichar nsHtml5NamedCharacters::NAME_1899[] = {
  's', 'u', 'n', 'g', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1899[] = {
  0x266a
};
PRUnichar nsHtml5NamedCharacters::NAME_1900[] = {
  's', 'u', 'p', '1'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1900[] = {
  0x00b9
};
PRUnichar nsHtml5NamedCharacters::NAME_1901[] = {
  's', 'u', 'p', '1', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1901[] = {
  0x00b9
};
PRUnichar nsHtml5NamedCharacters::NAME_1902[] = {
  's', 'u', 'p', '2'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1902[] = {
  0x00b2
};
PRUnichar nsHtml5NamedCharacters::NAME_1903[] = {
  's', 'u', 'p', '2', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1903[] = {
  0x00b2
};
PRUnichar nsHtml5NamedCharacters::NAME_1904[] = {
  's', 'u', 'p', '3'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1904[] = {
  0x00b3
};
PRUnichar nsHtml5NamedCharacters::NAME_1905[] = {
  's', 'u', 'p', '3', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1905[] = {
  0x00b3
};
PRUnichar nsHtml5NamedCharacters::NAME_1906[] = {
  's', 'u', 'p', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1906[] = {
  0x2283
};
PRUnichar nsHtml5NamedCharacters::NAME_1907[] = {
  's', 'u', 'p', 'E', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1907[] = {
  0x2ac6
};
PRUnichar nsHtml5NamedCharacters::NAME_1908[] = {
  's', 'u', 'p', 'd', 'o', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1908[] = {
  0x2abe
};
PRUnichar nsHtml5NamedCharacters::NAME_1909[] = {
  's', 'u', 'p', 'd', 's', 'u', 'b', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1909[] = {
  0x2ad8
};
PRUnichar nsHtml5NamedCharacters::NAME_1910[] = {
  's', 'u', 'p', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1910[] = {
  0x2287
};
PRUnichar nsHtml5NamedCharacters::NAME_1911[] = {
  's', 'u', 'p', 'e', 'd', 'o', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1911[] = {
  0x2ac4
};
PRUnichar nsHtml5NamedCharacters::NAME_1912[] = {
  's', 'u', 'p', 'h', 's', 'u', 'b', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1912[] = {
  0x2ad7
};
PRUnichar nsHtml5NamedCharacters::NAME_1913[] = {
  's', 'u', 'p', 'l', 'a', 'r', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1913[] = {
  0x297b
};
PRUnichar nsHtml5NamedCharacters::NAME_1914[] = {
  's', 'u', 'p', 'm', 'u', 'l', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1914[] = {
  0x2ac2
};
PRUnichar nsHtml5NamedCharacters::NAME_1915[] = {
  's', 'u', 'p', 'n', 'E', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1915[] = {
  0x2acc
};
PRUnichar nsHtml5NamedCharacters::NAME_1916[] = {
  's', 'u', 'p', 'n', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1916[] = {
  0x228b
};
PRUnichar nsHtml5NamedCharacters::NAME_1917[] = {
  's', 'u', 'p', 'p', 'l', 'u', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1917[] = {
  0x2ac0
};
PRUnichar nsHtml5NamedCharacters::NAME_1918[] = {
  's', 'u', 'p', 's', 'e', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1918[] = {
  0x2283
};
PRUnichar nsHtml5NamedCharacters::NAME_1919[] = {
  's', 'u', 'p', 's', 'e', 't', 'e', 'q', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1919[] = {
  0x2287
};
PRUnichar nsHtml5NamedCharacters::NAME_1920[] = {
  's', 'u', 'p', 's', 'e', 't', 'e', 'q', 'q', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1920[] = {
  0x2ac6
};
PRUnichar nsHtml5NamedCharacters::NAME_1921[] = {
  's', 'u', 'p', 's', 'e', 't', 'n', 'e', 'q', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1921[] = {
  0x228b
};
PRUnichar nsHtml5NamedCharacters::NAME_1922[] = {
  's', 'u', 'p', 's', 'e', 't', 'n', 'e', 'q', 'q', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1922[] = {
  0x2acc
};
PRUnichar nsHtml5NamedCharacters::NAME_1923[] = {
  's', 'u', 'p', 's', 'i', 'm', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1923[] = {
  0x2ac8
};
PRUnichar nsHtml5NamedCharacters::NAME_1924[] = {
  's', 'u', 'p', 's', 'u', 'b', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1924[] = {
  0x2ad4
};
PRUnichar nsHtml5NamedCharacters::NAME_1925[] = {
  's', 'u', 'p', 's', 'u', 'p', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1925[] = {
  0x2ad6
};
PRUnichar nsHtml5NamedCharacters::NAME_1926[] = {
  's', 'w', 'A', 'r', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1926[] = {
  0x21d9
};
PRUnichar nsHtml5NamedCharacters::NAME_1927[] = {
  's', 'w', 'a', 'r', 'h', 'k', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1927[] = {
  0x2926
};
PRUnichar nsHtml5NamedCharacters::NAME_1928[] = {
  's', 'w', 'a', 'r', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1928[] = {
  0x2199
};
PRUnichar nsHtml5NamedCharacters::NAME_1929[] = {
  's', 'w', 'a', 'r', 'r', 'o', 'w', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1929[] = {
  0x2199
};
PRUnichar nsHtml5NamedCharacters::NAME_1930[] = {
  's', 'w', 'n', 'w', 'a', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1930[] = {
  0x292a
};
PRUnichar nsHtml5NamedCharacters::NAME_1931[] = {
  's', 'z', 'l', 'i', 'g'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1931[] = {
  0x00df
};
PRUnichar nsHtml5NamedCharacters::NAME_1932[] = {
  's', 'z', 'l', 'i', 'g', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1932[] = {
  0x00df
};
PRUnichar nsHtml5NamedCharacters::NAME_1933[] = {
  't', 'a', 'r', 'g', 'e', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1933[] = {
  0x2316
};
PRUnichar nsHtml5NamedCharacters::NAME_1934[] = {
  't', 'a', 'u', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1934[] = {
  0x03c4
};
PRUnichar nsHtml5NamedCharacters::NAME_1935[] = {
  't', 'b', 'r', 'k', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1935[] = {
  0x23b4
};
PRUnichar nsHtml5NamedCharacters::NAME_1936[] = {
  't', 'c', 'a', 'r', 'o', 'n', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1936[] = {
  0x0165
};
PRUnichar nsHtml5NamedCharacters::NAME_1937[] = {
  't', 'c', 'e', 'd', 'i', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1937[] = {
  0x0163
};
PRUnichar nsHtml5NamedCharacters::NAME_1938[] = {
  't', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1938[] = {
  0x0442
};
PRUnichar nsHtml5NamedCharacters::NAME_1939[] = {
  't', 'd', 'o', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1939[] = {
  0x20db
};
PRUnichar nsHtml5NamedCharacters::NAME_1940[] = {
  't', 'e', 'l', 'r', 'e', 'c', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1940[] = {
  0x2315
};
PRUnichar nsHtml5NamedCharacters::NAME_1941[] = {
  't', 'f', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1941[] = {
  0xd835, 0xdd31
};
PRUnichar nsHtml5NamedCharacters::NAME_1942[] = {
  't', 'h', 'e', 'r', 'e', '4', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1942[] = {
  0x2234
};
PRUnichar nsHtml5NamedCharacters::NAME_1943[] = {
  't', 'h', 'e', 'r', 'e', 'f', 'o', 'r', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1943[] = {
  0x2234
};
PRUnichar nsHtml5NamedCharacters::NAME_1944[] = {
  't', 'h', 'e', 't', 'a', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1944[] = {
  0x03b8
};
PRUnichar nsHtml5NamedCharacters::NAME_1945[] = {
  't', 'h', 'e', 't', 'a', 's', 'y', 'm', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1945[] = {
  0x03d1
};
PRUnichar nsHtml5NamedCharacters::NAME_1946[] = {
  't', 'h', 'e', 't', 'a', 'v', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1946[] = {
  0x03d1
};
PRUnichar nsHtml5NamedCharacters::NAME_1947[] = {
  't', 'h', 'i', 'c', 'k', 'a', 'p', 'p', 'r', 'o', 'x', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1947[] = {
  0x2248
};
PRUnichar nsHtml5NamedCharacters::NAME_1948[] = {
  't', 'h', 'i', 'c', 'k', 's', 'i', 'm', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1948[] = {
  0x223c
};
PRUnichar nsHtml5NamedCharacters::NAME_1949[] = {
  't', 'h', 'i', 'n', 's', 'p', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1949[] = {
  0x2009
};
PRUnichar nsHtml5NamedCharacters::NAME_1950[] = {
  't', 'h', 'k', 'a', 'p', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1950[] = {
  0x2248
};
PRUnichar nsHtml5NamedCharacters::NAME_1951[] = {
  't', 'h', 'k', 's', 'i', 'm', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1951[] = {
  0x223c
};
PRUnichar nsHtml5NamedCharacters::NAME_1952[] = {
  't', 'h', 'o', 'r', 'n'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1952[] = {
  0x00fe
};
PRUnichar nsHtml5NamedCharacters::NAME_1953[] = {
  't', 'h', 'o', 'r', 'n', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1953[] = {
  0x00fe
};
PRUnichar nsHtml5NamedCharacters::NAME_1954[] = {
  't', 'i', 'l', 'd', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1954[] = {
  0x02dc
};
PRUnichar nsHtml5NamedCharacters::NAME_1955[] = {
  't', 'i', 'm', 'e', 's'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1955[] = {
  0x00d7
};
PRUnichar nsHtml5NamedCharacters::NAME_1956[] = {
  't', 'i', 'm', 'e', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1956[] = {
  0x00d7
};
PRUnichar nsHtml5NamedCharacters::NAME_1957[] = {
  't', 'i', 'm', 'e', 's', 'b', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1957[] = {
  0x22a0
};
PRUnichar nsHtml5NamedCharacters::NAME_1958[] = {
  't', 'i', 'm', 'e', 's', 'b', 'a', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1958[] = {
  0x2a31
};
PRUnichar nsHtml5NamedCharacters::NAME_1959[] = {
  't', 'i', 'm', 'e', 's', 'd', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1959[] = {
  0x2a30
};
PRUnichar nsHtml5NamedCharacters::NAME_1960[] = {
  't', 'i', 'n', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1960[] = {
  0x222d
};
PRUnichar nsHtml5NamedCharacters::NAME_1961[] = {
  't', 'o', 'e', 'a', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1961[] = {
  0x2928
};
PRUnichar nsHtml5NamedCharacters::NAME_1962[] = {
  't', 'o', 'p', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1962[] = {
  0x22a4
};
PRUnichar nsHtml5NamedCharacters::NAME_1963[] = {
  't', 'o', 'p', 'b', 'o', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1963[] = {
  0x2336
};
PRUnichar nsHtml5NamedCharacters::NAME_1964[] = {
  't', 'o', 'p', 'c', 'i', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1964[] = {
  0x2af1
};
PRUnichar nsHtml5NamedCharacters::NAME_1965[] = {
  't', 'o', 'p', 'f', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1965[] = {
  0xd835, 0xdd65
};
PRUnichar nsHtml5NamedCharacters::NAME_1966[] = {
  't', 'o', 'p', 'f', 'o', 'r', 'k', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1966[] = {
  0x2ada
};
PRUnichar nsHtml5NamedCharacters::NAME_1967[] = {
  't', 'o', 's', 'a', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1967[] = {
  0x2929
};
PRUnichar nsHtml5NamedCharacters::NAME_1968[] = {
  't', 'p', 'r', 'i', 'm', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1968[] = {
  0x2034
};
PRUnichar nsHtml5NamedCharacters::NAME_1969[] = {
  't', 'r', 'a', 'd', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1969[] = {
  0x2122
};
PRUnichar nsHtml5NamedCharacters::NAME_1970[] = {
  't', 'r', 'i', 'a', 'n', 'g', 'l', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1970[] = {
  0x25b5
};
PRUnichar nsHtml5NamedCharacters::NAME_1971[] = {
  't', 'r', 'i', 'a', 'n', 'g', 'l', 'e', 'd', 'o', 'w', 'n', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1971[] = {
  0x25bf
};
PRUnichar nsHtml5NamedCharacters::NAME_1972[] = {
  't', 'r', 'i', 'a', 'n', 'g', 'l', 'e', 'l', 'e', 'f', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1972[] = {
  0x25c3
};
PRUnichar nsHtml5NamedCharacters::NAME_1973[] = {
  't', 'r', 'i', 'a', 'n', 'g', 'l', 'e', 'l', 'e', 'f', 't', 'e', 'q', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1973[] = {
  0x22b4
};
PRUnichar nsHtml5NamedCharacters::NAME_1974[] = {
  't', 'r', 'i', 'a', 'n', 'g', 'l', 'e', 'q', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1974[] = {
  0x225c
};
PRUnichar nsHtml5NamedCharacters::NAME_1975[] = {
  't', 'r', 'i', 'a', 'n', 'g', 'l', 'e', 'r', 'i', 'g', 'h', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1975[] = {
  0x25b9
};
PRUnichar nsHtml5NamedCharacters::NAME_1976[] = {
  't', 'r', 'i', 'a', 'n', 'g', 'l', 'e', 'r', 'i', 'g', 'h', 't', 'e', 'q', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1976[] = {
  0x22b5
};
PRUnichar nsHtml5NamedCharacters::NAME_1977[] = {
  't', 'r', 'i', 'd', 'o', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1977[] = {
  0x25ec
};
PRUnichar nsHtml5NamedCharacters::NAME_1978[] = {
  't', 'r', 'i', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1978[] = {
  0x225c
};
PRUnichar nsHtml5NamedCharacters::NAME_1979[] = {
  't', 'r', 'i', 'm', 'i', 'n', 'u', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1979[] = {
  0x2a3a
};
PRUnichar nsHtml5NamedCharacters::NAME_1980[] = {
  't', 'r', 'i', 'p', 'l', 'u', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1980[] = {
  0x2a39
};
PRUnichar nsHtml5NamedCharacters::NAME_1981[] = {
  't', 'r', 'i', 's', 'b', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1981[] = {
  0x29cd
};
PRUnichar nsHtml5NamedCharacters::NAME_1982[] = {
  't', 'r', 'i', 't', 'i', 'm', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1982[] = {
  0x2a3b
};
PRUnichar nsHtml5NamedCharacters::NAME_1983[] = {
  't', 'r', 'p', 'e', 'z', 'i', 'u', 'm', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1983[] = {
  0x23e2
};
PRUnichar nsHtml5NamedCharacters::NAME_1984[] = {
  't', 's', 'c', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1984[] = {
  0xd835, 0xdcc9
};
PRUnichar nsHtml5NamedCharacters::NAME_1985[] = {
  't', 's', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1985[] = {
  0x0446
};
PRUnichar nsHtml5NamedCharacters::NAME_1986[] = {
  't', 's', 'h', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1986[] = {
  0x045b
};
PRUnichar nsHtml5NamedCharacters::NAME_1987[] = {
  't', 's', 't', 'r', 'o', 'k', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1987[] = {
  0x0167
};
PRUnichar nsHtml5NamedCharacters::NAME_1988[] = {
  't', 'w', 'i', 'x', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1988[] = {
  0x226c
};
PRUnichar nsHtml5NamedCharacters::NAME_1989[] = {
  't', 'w', 'o', 'h', 'e', 'a', 'd', 'l', 'e', 'f', 't', 'a', 'r', 'r', 'o', 'w', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1989[] = {
  0x219e
};
PRUnichar nsHtml5NamedCharacters::NAME_1990[] = {
  't', 'w', 'o', 'h', 'e', 'a', 'd', 'r', 'i', 'g', 'h', 't', 'a', 'r', 'r', 'o', 'w', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1990[] = {
  0x21a0
};
PRUnichar nsHtml5NamedCharacters::NAME_1991[] = {
  'u', 'A', 'r', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1991[] = {
  0x21d1
};
PRUnichar nsHtml5NamedCharacters::NAME_1992[] = {
  'u', 'H', 'a', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1992[] = {
  0x2963
};
PRUnichar nsHtml5NamedCharacters::NAME_1993[] = {
  'u', 'a', 'c', 'u', 't', 'e'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1993[] = {
  0x00fa
};
PRUnichar nsHtml5NamedCharacters::NAME_1994[] = {
  'u', 'a', 'c', 'u', 't', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1994[] = {
  0x00fa
};
PRUnichar nsHtml5NamedCharacters::NAME_1995[] = {
  'u', 'a', 'r', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1995[] = {
  0x2191
};
PRUnichar nsHtml5NamedCharacters::NAME_1996[] = {
  'u', 'b', 'r', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1996[] = {
  0x045e
};
PRUnichar nsHtml5NamedCharacters::NAME_1997[] = {
  'u', 'b', 'r', 'e', 'v', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1997[] = {
  0x016d
};
PRUnichar nsHtml5NamedCharacters::NAME_1998[] = {
  'u', 'c', 'i', 'r', 'c'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1998[] = {
  0x00fb
};
PRUnichar nsHtml5NamedCharacters::NAME_1999[] = {
  'u', 'c', 'i', 'r', 'c', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_1999[] = {
  0x00fb
};
PRUnichar nsHtml5NamedCharacters::NAME_2000[] = {
  'u', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2000[] = {
  0x0443
};
PRUnichar nsHtml5NamedCharacters::NAME_2001[] = {
  'u', 'd', 'a', 'r', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2001[] = {
  0x21c5
};
PRUnichar nsHtml5NamedCharacters::NAME_2002[] = {
  'u', 'd', 'b', 'l', 'a', 'c', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2002[] = {
  0x0171
};
PRUnichar nsHtml5NamedCharacters::NAME_2003[] = {
  'u', 'd', 'h', 'a', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2003[] = {
  0x296e
};
PRUnichar nsHtml5NamedCharacters::NAME_2004[] = {
  'u', 'f', 'i', 's', 'h', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2004[] = {
  0x297e
};
PRUnichar nsHtml5NamedCharacters::NAME_2005[] = {
  'u', 'f', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2005[] = {
  0xd835, 0xdd32
};
PRUnichar nsHtml5NamedCharacters::NAME_2006[] = {
  'u', 'g', 'r', 'a', 'v', 'e'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2006[] = {
  0x00f9
};
PRUnichar nsHtml5NamedCharacters::NAME_2007[] = {
  'u', 'g', 'r', 'a', 'v', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2007[] = {
  0x00f9
};
PRUnichar nsHtml5NamedCharacters::NAME_2008[] = {
  'u', 'h', 'a', 'r', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2008[] = {
  0x21bf
};
PRUnichar nsHtml5NamedCharacters::NAME_2009[] = {
  'u', 'h', 'a', 'r', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2009[] = {
  0x21be
};
PRUnichar nsHtml5NamedCharacters::NAME_2010[] = {
  'u', 'h', 'b', 'l', 'k', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2010[] = {
  0x2580
};
PRUnichar nsHtml5NamedCharacters::NAME_2011[] = {
  'u', 'l', 'c', 'o', 'r', 'n', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2011[] = {
  0x231c
};
PRUnichar nsHtml5NamedCharacters::NAME_2012[] = {
  'u', 'l', 'c', 'o', 'r', 'n', 'e', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2012[] = {
  0x231c
};
PRUnichar nsHtml5NamedCharacters::NAME_2013[] = {
  'u', 'l', 'c', 'r', 'o', 'p', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2013[] = {
  0x230f
};
PRUnichar nsHtml5NamedCharacters::NAME_2014[] = {
  'u', 'l', 't', 'r', 'i', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2014[] = {
  0x25f8
};
PRUnichar nsHtml5NamedCharacters::NAME_2015[] = {
  'u', 'm', 'a', 'c', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2015[] = {
  0x016b
};
PRUnichar nsHtml5NamedCharacters::NAME_2016[] = {
  'u', 'm', 'l'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2016[] = {
  0x00a8
};
PRUnichar nsHtml5NamedCharacters::NAME_2017[] = {
  'u', 'm', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2017[] = {
  0x00a8
};
PRUnichar nsHtml5NamedCharacters::NAME_2018[] = {
  'u', 'o', 'g', 'o', 'n', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2018[] = {
  0x0173
};
PRUnichar nsHtml5NamedCharacters::NAME_2019[] = {
  'u', 'o', 'p', 'f', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2019[] = {
  0xd835, 0xdd66
};
PRUnichar nsHtml5NamedCharacters::NAME_2020[] = {
  'u', 'p', 'a', 'r', 'r', 'o', 'w', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2020[] = {
  0x2191
};
PRUnichar nsHtml5NamedCharacters::NAME_2021[] = {
  'u', 'p', 'd', 'o', 'w', 'n', 'a', 'r', 'r', 'o', 'w', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2021[] = {
  0x2195
};
PRUnichar nsHtml5NamedCharacters::NAME_2022[] = {
  'u', 'p', 'h', 'a', 'r', 'p', 'o', 'o', 'n', 'l', 'e', 'f', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2022[] = {
  0x21bf
};
PRUnichar nsHtml5NamedCharacters::NAME_2023[] = {
  'u', 'p', 'h', 'a', 'r', 'p', 'o', 'o', 'n', 'r', 'i', 'g', 'h', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2023[] = {
  0x21be
};
PRUnichar nsHtml5NamedCharacters::NAME_2024[] = {
  'u', 'p', 'l', 'u', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2024[] = {
  0x228e
};
PRUnichar nsHtml5NamedCharacters::NAME_2025[] = {
  'u', 'p', 's', 'i', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2025[] = {
  0x03c5
};
PRUnichar nsHtml5NamedCharacters::NAME_2026[] = {
  'u', 'p', 's', 'i', 'h', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2026[] = {
  0x03d2
};
PRUnichar nsHtml5NamedCharacters::NAME_2027[] = {
  'u', 'p', 's', 'i', 'l', 'o', 'n', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2027[] = {
  0x03c5
};
PRUnichar nsHtml5NamedCharacters::NAME_2028[] = {
  'u', 'p', 'u', 'p', 'a', 'r', 'r', 'o', 'w', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2028[] = {
  0x21c8
};
PRUnichar nsHtml5NamedCharacters::NAME_2029[] = {
  'u', 'r', 'c', 'o', 'r', 'n', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2029[] = {
  0x231d
};
PRUnichar nsHtml5NamedCharacters::NAME_2030[] = {
  'u', 'r', 'c', 'o', 'r', 'n', 'e', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2030[] = {
  0x231d
};
PRUnichar nsHtml5NamedCharacters::NAME_2031[] = {
  'u', 'r', 'c', 'r', 'o', 'p', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2031[] = {
  0x230e
};
PRUnichar nsHtml5NamedCharacters::NAME_2032[] = {
  'u', 'r', 'i', 'n', 'g', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2032[] = {
  0x016f
};
PRUnichar nsHtml5NamedCharacters::NAME_2033[] = {
  'u', 'r', 't', 'r', 'i', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2033[] = {
  0x25f9
};
PRUnichar nsHtml5NamedCharacters::NAME_2034[] = {
  'u', 's', 'c', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2034[] = {
  0xd835, 0xdcca
};
PRUnichar nsHtml5NamedCharacters::NAME_2035[] = {
  'u', 't', 'd', 'o', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2035[] = {
  0x22f0
};
PRUnichar nsHtml5NamedCharacters::NAME_2036[] = {
  'u', 't', 'i', 'l', 'd', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2036[] = {
  0x0169
};
PRUnichar nsHtml5NamedCharacters::NAME_2037[] = {
  'u', 't', 'r', 'i', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2037[] = {
  0x25b5
};
PRUnichar nsHtml5NamedCharacters::NAME_2038[] = {
  'u', 't', 'r', 'i', 'f', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2038[] = {
  0x25b4
};
PRUnichar nsHtml5NamedCharacters::NAME_2039[] = {
  'u', 'u', 'a', 'r', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2039[] = {
  0x21c8
};
PRUnichar nsHtml5NamedCharacters::NAME_2040[] = {
  'u', 'u', 'm', 'l'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2040[] = {
  0x00fc
};
PRUnichar nsHtml5NamedCharacters::NAME_2041[] = {
  'u', 'u', 'm', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2041[] = {
  0x00fc
};
PRUnichar nsHtml5NamedCharacters::NAME_2042[] = {
  'u', 'w', 'a', 'n', 'g', 'l', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2042[] = {
  0x29a7
};
PRUnichar nsHtml5NamedCharacters::NAME_2043[] = {
  'v', 'A', 'r', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2043[] = {
  0x21d5
};
PRUnichar nsHtml5NamedCharacters::NAME_2044[] = {
  'v', 'B', 'a', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2044[] = {
  0x2ae8
};
PRUnichar nsHtml5NamedCharacters::NAME_2045[] = {
  'v', 'B', 'a', 'r', 'v', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2045[] = {
  0x2ae9
};
PRUnichar nsHtml5NamedCharacters::NAME_2046[] = {
  'v', 'D', 'a', 's', 'h', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2046[] = {
  0x22a8
};
PRUnichar nsHtml5NamedCharacters::NAME_2047[] = {
  'v', 'a', 'n', 'g', 'r', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2047[] = {
  0x299c
};
PRUnichar nsHtml5NamedCharacters::NAME_2048[] = {
  'v', 'a', 'r', 'e', 'p', 's', 'i', 'l', 'o', 'n', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2048[] = {
  0x03b5
};
PRUnichar nsHtml5NamedCharacters::NAME_2049[] = {
  'v', 'a', 'r', 'k', 'a', 'p', 'p', 'a', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2049[] = {
  0x03f0
};
PRUnichar nsHtml5NamedCharacters::NAME_2050[] = {
  'v', 'a', 'r', 'n', 'o', 't', 'h', 'i', 'n', 'g', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2050[] = {
  0x2205
};
PRUnichar nsHtml5NamedCharacters::NAME_2051[] = {
  'v', 'a', 'r', 'p', 'h', 'i', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2051[] = {
  0x03c6
};
PRUnichar nsHtml5NamedCharacters::NAME_2052[] = {
  'v', 'a', 'r', 'p', 'i', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2052[] = {
  0x03d6
};
PRUnichar nsHtml5NamedCharacters::NAME_2053[] = {
  'v', 'a', 'r', 'p', 'r', 'o', 'p', 't', 'o', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2053[] = {
  0x221d
};
PRUnichar nsHtml5NamedCharacters::NAME_2054[] = {
  'v', 'a', 'r', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2054[] = {
  0x2195
};
PRUnichar nsHtml5NamedCharacters::NAME_2055[] = {
  'v', 'a', 'r', 'r', 'h', 'o', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2055[] = {
  0x03f1
};
PRUnichar nsHtml5NamedCharacters::NAME_2056[] = {
  'v', 'a', 'r', 's', 'i', 'g', 'm', 'a', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2056[] = {
  0x03c2
};
PRUnichar nsHtml5NamedCharacters::NAME_2057[] = {
  'v', 'a', 'r', 't', 'h', 'e', 't', 'a', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2057[] = {
  0x03d1
};
PRUnichar nsHtml5NamedCharacters::NAME_2058[] = {
  'v', 'a', 'r', 't', 'r', 'i', 'a', 'n', 'g', 'l', 'e', 'l', 'e', 'f', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2058[] = {
  0x22b2
};
PRUnichar nsHtml5NamedCharacters::NAME_2059[] = {
  'v', 'a', 'r', 't', 'r', 'i', 'a', 'n', 'g', 'l', 'e', 'r', 'i', 'g', 'h', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2059[] = {
  0x22b3
};
PRUnichar nsHtml5NamedCharacters::NAME_2060[] = {
  'v', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2060[] = {
  0x0432
};
PRUnichar nsHtml5NamedCharacters::NAME_2061[] = {
  'v', 'd', 'a', 's', 'h', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2061[] = {
  0x22a2
};
PRUnichar nsHtml5NamedCharacters::NAME_2062[] = {
  'v', 'e', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2062[] = {
  0x2228
};
PRUnichar nsHtml5NamedCharacters::NAME_2063[] = {
  'v', 'e', 'e', 'b', 'a', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2063[] = {
  0x22bb
};
PRUnichar nsHtml5NamedCharacters::NAME_2064[] = {
  'v', 'e', 'e', 'e', 'q', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2064[] = {
  0x225a
};
PRUnichar nsHtml5NamedCharacters::NAME_2065[] = {
  'v', 'e', 'l', 'l', 'i', 'p', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2065[] = {
  0x22ee
};
PRUnichar nsHtml5NamedCharacters::NAME_2066[] = {
  'v', 'e', 'r', 'b', 'a', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2066[] = {
  0x007c
};
PRUnichar nsHtml5NamedCharacters::NAME_2067[] = {
  'v', 'e', 'r', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2067[] = {
  0x007c
};
PRUnichar nsHtml5NamedCharacters::NAME_2068[] = {
  'v', 'f', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2068[] = {
  0xd835, 0xdd33
};
PRUnichar nsHtml5NamedCharacters::NAME_2069[] = {
  'v', 'l', 't', 'r', 'i', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2069[] = {
  0x22b2
};
PRUnichar nsHtml5NamedCharacters::NAME_2070[] = {
  'v', 'o', 'p', 'f', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2070[] = {
  0xd835, 0xdd67
};
PRUnichar nsHtml5NamedCharacters::NAME_2071[] = {
  'v', 'p', 'r', 'o', 'p', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2071[] = {
  0x221d
};
PRUnichar nsHtml5NamedCharacters::NAME_2072[] = {
  'v', 'r', 't', 'r', 'i', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2072[] = {
  0x22b3
};
PRUnichar nsHtml5NamedCharacters::NAME_2073[] = {
  'v', 's', 'c', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2073[] = {
  0xd835, 0xdccb
};
PRUnichar nsHtml5NamedCharacters::NAME_2074[] = {
  'v', 'z', 'i', 'g', 'z', 'a', 'g', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2074[] = {
  0x299a
};
PRUnichar nsHtml5NamedCharacters::NAME_2075[] = {
  'w', 'c', 'i', 'r', 'c', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2075[] = {
  0x0175
};
PRUnichar nsHtml5NamedCharacters::NAME_2076[] = {
  'w', 'e', 'd', 'b', 'a', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2076[] = {
  0x2a5f
};
PRUnichar nsHtml5NamedCharacters::NAME_2077[] = {
  'w', 'e', 'd', 'g', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2077[] = {
  0x2227
};
PRUnichar nsHtml5NamedCharacters::NAME_2078[] = {
  'w', 'e', 'd', 'g', 'e', 'q', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2078[] = {
  0x2259
};
PRUnichar nsHtml5NamedCharacters::NAME_2079[] = {
  'w', 'e', 'i', 'e', 'r', 'p', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2079[] = {
  0x2118
};
PRUnichar nsHtml5NamedCharacters::NAME_2080[] = {
  'w', 'f', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2080[] = {
  0xd835, 0xdd34
};
PRUnichar nsHtml5NamedCharacters::NAME_2081[] = {
  'w', 'o', 'p', 'f', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2081[] = {
  0xd835, 0xdd68
};
PRUnichar nsHtml5NamedCharacters::NAME_2082[] = {
  'w', 'p', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2082[] = {
  0x2118
};
PRUnichar nsHtml5NamedCharacters::NAME_2083[] = {
  'w', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2083[] = {
  0x2240
};
PRUnichar nsHtml5NamedCharacters::NAME_2084[] = {
  'w', 'r', 'e', 'a', 't', 'h', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2084[] = {
  0x2240
};
PRUnichar nsHtml5NamedCharacters::NAME_2085[] = {
  'w', 's', 'c', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2085[] = {
  0xd835, 0xdccc
};
PRUnichar nsHtml5NamedCharacters::NAME_2086[] = {
  'x', 'c', 'a', 'p', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2086[] = {
  0x22c2
};
PRUnichar nsHtml5NamedCharacters::NAME_2087[] = {
  'x', 'c', 'i', 'r', 'c', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2087[] = {
  0x25ef
};
PRUnichar nsHtml5NamedCharacters::NAME_2088[] = {
  'x', 'c', 'u', 'p', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2088[] = {
  0x22c3
};
PRUnichar nsHtml5NamedCharacters::NAME_2089[] = {
  'x', 'd', 't', 'r', 'i', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2089[] = {
  0x25bd
};
PRUnichar nsHtml5NamedCharacters::NAME_2090[] = {
  'x', 'f', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2090[] = {
  0xd835, 0xdd35
};
PRUnichar nsHtml5NamedCharacters::NAME_2091[] = {
  'x', 'h', 'A', 'r', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2091[] = {
  0x27fa
};
PRUnichar nsHtml5NamedCharacters::NAME_2092[] = {
  'x', 'h', 'a', 'r', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2092[] = {
  0x27f7
};
PRUnichar nsHtml5NamedCharacters::NAME_2093[] = {
  'x', 'i', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2093[] = {
  0x03be
};
PRUnichar nsHtml5NamedCharacters::NAME_2094[] = {
  'x', 'l', 'A', 'r', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2094[] = {
  0x27f8
};
PRUnichar nsHtml5NamedCharacters::NAME_2095[] = {
  'x', 'l', 'a', 'r', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2095[] = {
  0x27f5
};
PRUnichar nsHtml5NamedCharacters::NAME_2096[] = {
  'x', 'm', 'a', 'p', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2096[] = {
  0x27fc
};
PRUnichar nsHtml5NamedCharacters::NAME_2097[] = {
  'x', 'n', 'i', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2097[] = {
  0x22fb
};
PRUnichar nsHtml5NamedCharacters::NAME_2098[] = {
  'x', 'o', 'd', 'o', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2098[] = {
  0x2a00
};
PRUnichar nsHtml5NamedCharacters::NAME_2099[] = {
  'x', 'o', 'p', 'f', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2099[] = {
  0xd835, 0xdd69
};
PRUnichar nsHtml5NamedCharacters::NAME_2100[] = {
  'x', 'o', 'p', 'l', 'u', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2100[] = {
  0x2a01
};
PRUnichar nsHtml5NamedCharacters::NAME_2101[] = {
  'x', 'o', 't', 'i', 'm', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2101[] = {
  0x2a02
};
PRUnichar nsHtml5NamedCharacters::NAME_2102[] = {
  'x', 'r', 'A', 'r', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2102[] = {
  0x27f9
};
PRUnichar nsHtml5NamedCharacters::NAME_2103[] = {
  'x', 'r', 'a', 'r', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2103[] = {
  0x27f6
};
PRUnichar nsHtml5NamedCharacters::NAME_2104[] = {
  'x', 's', 'c', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2104[] = {
  0xd835, 0xdccd
};
PRUnichar nsHtml5NamedCharacters::NAME_2105[] = {
  'x', 's', 'q', 'c', 'u', 'p', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2105[] = {
  0x2a06
};
PRUnichar nsHtml5NamedCharacters::NAME_2106[] = {
  'x', 'u', 'p', 'l', 'u', 's', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2106[] = {
  0x2a04
};
PRUnichar nsHtml5NamedCharacters::NAME_2107[] = {
  'x', 'u', 't', 'r', 'i', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2107[] = {
  0x25b3
};
PRUnichar nsHtml5NamedCharacters::NAME_2108[] = {
  'x', 'v', 'e', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2108[] = {
  0x22c1
};
PRUnichar nsHtml5NamedCharacters::NAME_2109[] = {
  'x', 'w', 'e', 'd', 'g', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2109[] = {
  0x22c0
};
PRUnichar nsHtml5NamedCharacters::NAME_2110[] = {
  'y', 'a', 'c', 'u', 't', 'e'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2110[] = {
  0x00fd
};
PRUnichar nsHtml5NamedCharacters::NAME_2111[] = {
  'y', 'a', 'c', 'u', 't', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2111[] = {
  0x00fd
};
PRUnichar nsHtml5NamedCharacters::NAME_2112[] = {
  'y', 'a', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2112[] = {
  0x044f
};
PRUnichar nsHtml5NamedCharacters::NAME_2113[] = {
  'y', 'c', 'i', 'r', 'c', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2113[] = {
  0x0177
};
PRUnichar nsHtml5NamedCharacters::NAME_2114[] = {
  'y', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2114[] = {
  0x044b
};
PRUnichar nsHtml5NamedCharacters::NAME_2115[] = {
  'y', 'e', 'n'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2115[] = {
  0x00a5
};
PRUnichar nsHtml5NamedCharacters::NAME_2116[] = {
  'y', 'e', 'n', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2116[] = {
  0x00a5
};
PRUnichar nsHtml5NamedCharacters::NAME_2117[] = {
  'y', 'f', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2117[] = {
  0xd835, 0xdd36
};
PRUnichar nsHtml5NamedCharacters::NAME_2118[] = {
  'y', 'i', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2118[] = {
  0x0457
};
PRUnichar nsHtml5NamedCharacters::NAME_2119[] = {
  'y', 'o', 'p', 'f', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2119[] = {
  0xd835, 0xdd6a
};
PRUnichar nsHtml5NamedCharacters::NAME_2120[] = {
  'y', 's', 'c', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2120[] = {
  0xd835, 0xdcce
};
PRUnichar nsHtml5NamedCharacters::NAME_2121[] = {
  'y', 'u', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2121[] = {
  0x044e
};
PRUnichar nsHtml5NamedCharacters::NAME_2122[] = {
  'y', 'u', 'm', 'l'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2122[] = {
  0x00ff
};
PRUnichar nsHtml5NamedCharacters::NAME_2123[] = {
  'y', 'u', 'm', 'l', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2123[] = {
  0x00ff
};
PRUnichar nsHtml5NamedCharacters::NAME_2124[] = {
  'z', 'a', 'c', 'u', 't', 'e', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2124[] = {
  0x017a
};
PRUnichar nsHtml5NamedCharacters::NAME_2125[] = {
  'z', 'c', 'a', 'r', 'o', 'n', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2125[] = {
  0x017e
};
PRUnichar nsHtml5NamedCharacters::NAME_2126[] = {
  'z', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2126[] = {
  0x0437
};
PRUnichar nsHtml5NamedCharacters::NAME_2127[] = {
  'z', 'd', 'o', 't', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2127[] = {
  0x017c
};
PRUnichar nsHtml5NamedCharacters::NAME_2128[] = {
  'z', 'e', 'e', 't', 'r', 'f', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2128[] = {
  0x2128
};
PRUnichar nsHtml5NamedCharacters::NAME_2129[] = {
  'z', 'e', 't', 'a', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2129[] = {
  0x03b6
};
PRUnichar nsHtml5NamedCharacters::NAME_2130[] = {
  'z', 'f', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2130[] = {
  0xd835, 0xdd37
};
PRUnichar nsHtml5NamedCharacters::NAME_2131[] = {
  'z', 'h', 'c', 'y', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2131[] = {
  0x0436
};
PRUnichar nsHtml5NamedCharacters::NAME_2132[] = {
  'z', 'i', 'g', 'r', 'a', 'r', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2132[] = {
  0x21dd
};
PRUnichar nsHtml5NamedCharacters::NAME_2133[] = {
  'z', 'o', 'p', 'f', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2133[] = {
  0xd835, 0xdd6b
};
PRUnichar nsHtml5NamedCharacters::NAME_2134[] = {
  'z', 's', 'c', 'r', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2134[] = {
  0xd835, 0xdccf
};
PRUnichar nsHtml5NamedCharacters::NAME_2135[] = {
  'z', 'w', 'j', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2135[] = {
  0x200d
};
PRUnichar nsHtml5NamedCharacters::NAME_2136[] = {
  'z', 'w', 'n', 'j', ';'
};
PRUnichar nsHtml5NamedCharacters::VALUE_2136[] = {
  0x200c
};
void
nsHtml5NamedCharacters::initializeStatics()
{
  NAMES = jArray<jArray<PRUnichar,PRInt32>,PRInt32>(2137);
  NAMES[0] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_0);
  NAMES[1] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1);
  NAMES[2] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2);
  NAMES[3] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_3);
  NAMES[4] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_4);
  NAMES[5] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_5);
  NAMES[6] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_6);
  NAMES[7] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_7);
  NAMES[8] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_8);
  NAMES[9] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_9);
  NAMES[10] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_10);
  NAMES[11] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_11);
  NAMES[12] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_12);
  NAMES[13] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_13);
  NAMES[14] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_14);
  NAMES[15] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_15);
  NAMES[16] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_16);
  NAMES[17] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_17);
  NAMES[18] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_18);
  NAMES[19] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_19);
  NAMES[20] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_20);
  NAMES[21] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_21);
  NAMES[22] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_22);
  NAMES[23] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_23);
  NAMES[24] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_24);
  NAMES[25] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_25);
  NAMES[26] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_26);
  NAMES[27] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_27);
  NAMES[28] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_28);
  NAMES[29] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_29);
  NAMES[30] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_30);
  NAMES[31] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_31);
  NAMES[32] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_32);
  NAMES[33] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_33);
  NAMES[34] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_34);
  NAMES[35] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_35);
  NAMES[36] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_36);
  NAMES[37] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_37);
  NAMES[38] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_38);
  NAMES[39] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_39);
  NAMES[40] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_40);
  NAMES[41] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_41);
  NAMES[42] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_42);
  NAMES[43] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_43);
  NAMES[44] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_44);
  NAMES[45] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_45);
  NAMES[46] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_46);
  NAMES[47] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_47);
  NAMES[48] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_48);
  NAMES[49] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_49);
  NAMES[50] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_50);
  NAMES[51] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_51);
  NAMES[52] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_52);
  NAMES[53] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_53);
  NAMES[54] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_54);
  NAMES[55] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_55);
  NAMES[56] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_56);
  NAMES[57] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_57);
  NAMES[58] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_58);
  NAMES[59] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_59);
  NAMES[60] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_60);
  NAMES[61] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_61);
  NAMES[62] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_62);
  NAMES[63] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_63);
  NAMES[64] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_64);
  NAMES[65] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_65);
  NAMES[66] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_66);
  NAMES[67] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_67);
  NAMES[68] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_68);
  NAMES[69] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_69);
  NAMES[70] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_70);
  NAMES[71] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_71);
  NAMES[72] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_72);
  NAMES[73] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_73);
  NAMES[74] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_74);
  NAMES[75] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_75);
  NAMES[76] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_76);
  NAMES[77] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_77);
  NAMES[78] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_78);
  NAMES[79] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_79);
  NAMES[80] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_80);
  NAMES[81] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_81);
  NAMES[82] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_82);
  NAMES[83] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_83);
  NAMES[84] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_84);
  NAMES[85] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_85);
  NAMES[86] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_86);
  NAMES[87] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_87);
  NAMES[88] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_88);
  NAMES[89] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_89);
  NAMES[90] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_90);
  NAMES[91] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_91);
  NAMES[92] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_92);
  NAMES[93] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_93);
  NAMES[94] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_94);
  NAMES[95] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_95);
  NAMES[96] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_96);
  NAMES[97] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_97);
  NAMES[98] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_98);
  NAMES[99] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_99);
  NAMES[100] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_100);
  NAMES[101] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_101);
  NAMES[102] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_102);
  NAMES[103] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_103);
  NAMES[104] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_104);
  NAMES[105] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_105);
  NAMES[106] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_106);
  NAMES[107] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_107);
  NAMES[108] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_108);
  NAMES[109] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_109);
  NAMES[110] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_110);
  NAMES[111] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_111);
  NAMES[112] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_112);
  NAMES[113] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_113);
  NAMES[114] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_114);
  NAMES[115] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_115);
  NAMES[116] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_116);
  NAMES[117] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_117);
  NAMES[118] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_118);
  NAMES[119] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_119);
  NAMES[120] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_120);
  NAMES[121] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_121);
  NAMES[122] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_122);
  NAMES[123] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_123);
  NAMES[124] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_124);
  NAMES[125] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_125);
  NAMES[126] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_126);
  NAMES[127] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_127);
  NAMES[128] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_128);
  NAMES[129] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_129);
  NAMES[130] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_130);
  NAMES[131] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_131);
  NAMES[132] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_132);
  NAMES[133] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_133);
  NAMES[134] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_134);
  NAMES[135] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_135);
  NAMES[136] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_136);
  NAMES[137] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_137);
  NAMES[138] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_138);
  NAMES[139] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_139);
  NAMES[140] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_140);
  NAMES[141] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_141);
  NAMES[142] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_142);
  NAMES[143] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_143);
  NAMES[144] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_144);
  NAMES[145] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_145);
  NAMES[146] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_146);
  NAMES[147] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_147);
  NAMES[148] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_148);
  NAMES[149] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_149);
  NAMES[150] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_150);
  NAMES[151] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_151);
  NAMES[152] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_152);
  NAMES[153] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_153);
  NAMES[154] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_154);
  NAMES[155] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_155);
  NAMES[156] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_156);
  NAMES[157] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_157);
  NAMES[158] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_158);
  NAMES[159] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_159);
  NAMES[160] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_160);
  NAMES[161] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_161);
  NAMES[162] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_162);
  NAMES[163] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_163);
  NAMES[164] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_164);
  NAMES[165] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_165);
  NAMES[166] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_166);
  NAMES[167] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_167);
  NAMES[168] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_168);
  NAMES[169] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_169);
  NAMES[170] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_170);
  NAMES[171] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_171);
  NAMES[172] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_172);
  NAMES[173] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_173);
  NAMES[174] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_174);
  NAMES[175] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_175);
  NAMES[176] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_176);
  NAMES[177] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_177);
  NAMES[178] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_178);
  NAMES[179] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_179);
  NAMES[180] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_180);
  NAMES[181] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_181);
  NAMES[182] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_182);
  NAMES[183] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_183);
  NAMES[184] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_184);
  NAMES[185] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_185);
  NAMES[186] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_186);
  NAMES[187] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_187);
  NAMES[188] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_188);
  NAMES[189] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_189);
  NAMES[190] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_190);
  NAMES[191] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_191);
  NAMES[192] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_192);
  NAMES[193] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_193);
  NAMES[194] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_194);
  NAMES[195] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_195);
  NAMES[196] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_196);
  NAMES[197] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_197);
  NAMES[198] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_198);
  NAMES[199] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_199);
  NAMES[200] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_200);
  NAMES[201] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_201);
  NAMES[202] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_202);
  NAMES[203] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_203);
  NAMES[204] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_204);
  NAMES[205] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_205);
  NAMES[206] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_206);
  NAMES[207] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_207);
  NAMES[208] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_208);
  NAMES[209] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_209);
  NAMES[210] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_210);
  NAMES[211] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_211);
  NAMES[212] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_212);
  NAMES[213] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_213);
  NAMES[214] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_214);
  NAMES[215] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_215);
  NAMES[216] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_216);
  NAMES[217] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_217);
  NAMES[218] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_218);
  NAMES[219] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_219);
  NAMES[220] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_220);
  NAMES[221] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_221);
  NAMES[222] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_222);
  NAMES[223] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_223);
  NAMES[224] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_224);
  NAMES[225] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_225);
  NAMES[226] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_226);
  NAMES[227] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_227);
  NAMES[228] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_228);
  NAMES[229] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_229);
  NAMES[230] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_230);
  NAMES[231] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_231);
  NAMES[232] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_232);
  NAMES[233] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_233);
  NAMES[234] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_234);
  NAMES[235] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_235);
  NAMES[236] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_236);
  NAMES[237] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_237);
  NAMES[238] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_238);
  NAMES[239] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_239);
  NAMES[240] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_240);
  NAMES[241] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_241);
  NAMES[242] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_242);
  NAMES[243] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_243);
  NAMES[244] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_244);
  NAMES[245] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_245);
  NAMES[246] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_246);
  NAMES[247] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_247);
  NAMES[248] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_248);
  NAMES[249] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_249);
  NAMES[250] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_250);
  NAMES[251] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_251);
  NAMES[252] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_252);
  NAMES[253] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_253);
  NAMES[254] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_254);
  NAMES[255] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_255);
  NAMES[256] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_256);
  NAMES[257] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_257);
  NAMES[258] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_258);
  NAMES[259] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_259);
  NAMES[260] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_260);
  NAMES[261] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_261);
  NAMES[262] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_262);
  NAMES[263] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_263);
  NAMES[264] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_264);
  NAMES[265] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_265);
  NAMES[266] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_266);
  NAMES[267] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_267);
  NAMES[268] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_268);
  NAMES[269] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_269);
  NAMES[270] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_270);
  NAMES[271] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_271);
  NAMES[272] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_272);
  NAMES[273] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_273);
  NAMES[274] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_274);
  NAMES[275] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_275);
  NAMES[276] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_276);
  NAMES[277] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_277);
  NAMES[278] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_278);
  NAMES[279] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_279);
  NAMES[280] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_280);
  NAMES[281] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_281);
  NAMES[282] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_282);
  NAMES[283] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_283);
  NAMES[284] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_284);
  NAMES[285] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_285);
  NAMES[286] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_286);
  NAMES[287] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_287);
  NAMES[288] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_288);
  NAMES[289] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_289);
  NAMES[290] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_290);
  NAMES[291] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_291);
  NAMES[292] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_292);
  NAMES[293] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_293);
  NAMES[294] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_294);
  NAMES[295] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_295);
  NAMES[296] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_296);
  NAMES[297] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_297);
  NAMES[298] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_298);
  NAMES[299] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_299);
  NAMES[300] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_300);
  NAMES[301] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_301);
  NAMES[302] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_302);
  NAMES[303] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_303);
  NAMES[304] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_304);
  NAMES[305] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_305);
  NAMES[306] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_306);
  NAMES[307] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_307);
  NAMES[308] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_308);
  NAMES[309] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_309);
  NAMES[310] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_310);
  NAMES[311] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_311);
  NAMES[312] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_312);
  NAMES[313] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_313);
  NAMES[314] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_314);
  NAMES[315] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_315);
  NAMES[316] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_316);
  NAMES[317] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_317);
  NAMES[318] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_318);
  NAMES[319] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_319);
  NAMES[320] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_320);
  NAMES[321] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_321);
  NAMES[322] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_322);
  NAMES[323] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_323);
  NAMES[324] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_324);
  NAMES[325] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_325);
  NAMES[326] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_326);
  NAMES[327] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_327);
  NAMES[328] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_328);
  NAMES[329] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_329);
  NAMES[330] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_330);
  NAMES[331] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_331);
  NAMES[332] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_332);
  NAMES[333] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_333);
  NAMES[334] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_334);
  NAMES[335] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_335);
  NAMES[336] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_336);
  NAMES[337] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_337);
  NAMES[338] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_338);
  NAMES[339] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_339);
  NAMES[340] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_340);
  NAMES[341] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_341);
  NAMES[342] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_342);
  NAMES[343] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_343);
  NAMES[344] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_344);
  NAMES[345] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_345);
  NAMES[346] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_346);
  NAMES[347] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_347);
  NAMES[348] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_348);
  NAMES[349] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_349);
  NAMES[350] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_350);
  NAMES[351] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_351);
  NAMES[352] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_352);
  NAMES[353] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_353);
  NAMES[354] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_354);
  NAMES[355] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_355);
  NAMES[356] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_356);
  NAMES[357] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_357);
  NAMES[358] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_358);
  NAMES[359] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_359);
  NAMES[360] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_360);
  NAMES[361] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_361);
  NAMES[362] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_362);
  NAMES[363] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_363);
  NAMES[364] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_364);
  NAMES[365] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_365);
  NAMES[366] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_366);
  NAMES[367] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_367);
  NAMES[368] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_368);
  NAMES[369] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_369);
  NAMES[370] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_370);
  NAMES[371] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_371);
  NAMES[372] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_372);
  NAMES[373] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_373);
  NAMES[374] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_374);
  NAMES[375] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_375);
  NAMES[376] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_376);
  NAMES[377] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_377);
  NAMES[378] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_378);
  NAMES[379] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_379);
  NAMES[380] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_380);
  NAMES[381] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_381);
  NAMES[382] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_382);
  NAMES[383] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_383);
  NAMES[384] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_384);
  NAMES[385] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_385);
  NAMES[386] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_386);
  NAMES[387] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_387);
  NAMES[388] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_388);
  NAMES[389] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_389);
  NAMES[390] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_390);
  NAMES[391] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_391);
  NAMES[392] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_392);
  NAMES[393] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_393);
  NAMES[394] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_394);
  NAMES[395] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_395);
  NAMES[396] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_396);
  NAMES[397] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_397);
  NAMES[398] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_398);
  NAMES[399] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_399);
  NAMES[400] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_400);
  NAMES[401] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_401);
  NAMES[402] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_402);
  NAMES[403] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_403);
  NAMES[404] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_404);
  NAMES[405] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_405);
  NAMES[406] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_406);
  NAMES[407] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_407);
  NAMES[408] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_408);
  NAMES[409] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_409);
  NAMES[410] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_410);
  NAMES[411] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_411);
  NAMES[412] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_412);
  NAMES[413] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_413);
  NAMES[414] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_414);
  NAMES[415] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_415);
  NAMES[416] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_416);
  NAMES[417] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_417);
  NAMES[418] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_418);
  NAMES[419] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_419);
  NAMES[420] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_420);
  NAMES[421] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_421);
  NAMES[422] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_422);
  NAMES[423] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_423);
  NAMES[424] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_424);
  NAMES[425] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_425);
  NAMES[426] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_426);
  NAMES[427] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_427);
  NAMES[428] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_428);
  NAMES[429] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_429);
  NAMES[430] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_430);
  NAMES[431] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_431);
  NAMES[432] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_432);
  NAMES[433] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_433);
  NAMES[434] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_434);
  NAMES[435] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_435);
  NAMES[436] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_436);
  NAMES[437] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_437);
  NAMES[438] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_438);
  NAMES[439] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_439);
  NAMES[440] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_440);
  NAMES[441] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_441);
  NAMES[442] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_442);
  NAMES[443] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_443);
  NAMES[444] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_444);
  NAMES[445] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_445);
  NAMES[446] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_446);
  NAMES[447] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_447);
  NAMES[448] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_448);
  NAMES[449] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_449);
  NAMES[450] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_450);
  NAMES[451] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_451);
  NAMES[452] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_452);
  NAMES[453] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_453);
  NAMES[454] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_454);
  NAMES[455] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_455);
  NAMES[456] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_456);
  NAMES[457] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_457);
  NAMES[458] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_458);
  NAMES[459] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_459);
  NAMES[460] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_460);
  NAMES[461] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_461);
  NAMES[462] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_462);
  NAMES[463] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_463);
  NAMES[464] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_464);
  NAMES[465] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_465);
  NAMES[466] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_466);
  NAMES[467] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_467);
  NAMES[468] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_468);
  NAMES[469] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_469);
  NAMES[470] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_470);
  NAMES[471] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_471);
  NAMES[472] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_472);
  NAMES[473] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_473);
  NAMES[474] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_474);
  NAMES[475] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_475);
  NAMES[476] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_476);
  NAMES[477] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_477);
  NAMES[478] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_478);
  NAMES[479] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_479);
  NAMES[480] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_480);
  NAMES[481] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_481);
  NAMES[482] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_482);
  NAMES[483] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_483);
  NAMES[484] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_484);
  NAMES[485] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_485);
  NAMES[486] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_486);
  NAMES[487] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_487);
  NAMES[488] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_488);
  NAMES[489] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_489);
  NAMES[490] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_490);
  NAMES[491] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_491);
  NAMES[492] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_492);
  NAMES[493] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_493);
  NAMES[494] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_494);
  NAMES[495] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_495);
  NAMES[496] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_496);
  NAMES[497] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_497);
  NAMES[498] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_498);
  NAMES[499] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_499);
  NAMES[500] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_500);
  NAMES[501] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_501);
  NAMES[502] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_502);
  NAMES[503] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_503);
  NAMES[504] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_504);
  NAMES[505] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_505);
  NAMES[506] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_506);
  NAMES[507] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_507);
  NAMES[508] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_508);
  NAMES[509] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_509);
  NAMES[510] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_510);
  NAMES[511] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_511);
  NAMES[512] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_512);
  NAMES[513] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_513);
  NAMES[514] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_514);
  NAMES[515] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_515);
  NAMES[516] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_516);
  NAMES[517] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_517);
  NAMES[518] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_518);
  NAMES[519] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_519);
  NAMES[520] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_520);
  NAMES[521] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_521);
  NAMES[522] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_522);
  NAMES[523] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_523);
  NAMES[524] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_524);
  NAMES[525] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_525);
  NAMES[526] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_526);
  NAMES[527] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_527);
  NAMES[528] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_528);
  NAMES[529] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_529);
  NAMES[530] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_530);
  NAMES[531] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_531);
  NAMES[532] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_532);
  NAMES[533] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_533);
  NAMES[534] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_534);
  NAMES[535] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_535);
  NAMES[536] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_536);
  NAMES[537] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_537);
  NAMES[538] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_538);
  NAMES[539] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_539);
  NAMES[540] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_540);
  NAMES[541] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_541);
  NAMES[542] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_542);
  NAMES[543] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_543);
  NAMES[544] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_544);
  NAMES[545] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_545);
  NAMES[546] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_546);
  NAMES[547] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_547);
  NAMES[548] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_548);
  NAMES[549] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_549);
  NAMES[550] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_550);
  NAMES[551] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_551);
  NAMES[552] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_552);
  NAMES[553] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_553);
  NAMES[554] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_554);
  NAMES[555] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_555);
  NAMES[556] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_556);
  NAMES[557] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_557);
  NAMES[558] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_558);
  NAMES[559] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_559);
  NAMES[560] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_560);
  NAMES[561] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_561);
  NAMES[562] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_562);
  NAMES[563] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_563);
  NAMES[564] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_564);
  NAMES[565] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_565);
  NAMES[566] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_566);
  NAMES[567] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_567);
  NAMES[568] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_568);
  NAMES[569] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_569);
  NAMES[570] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_570);
  NAMES[571] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_571);
  NAMES[572] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_572);
  NAMES[573] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_573);
  NAMES[574] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_574);
  NAMES[575] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_575);
  NAMES[576] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_576);
  NAMES[577] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_577);
  NAMES[578] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_578);
  NAMES[579] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_579);
  NAMES[580] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_580);
  NAMES[581] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_581);
  NAMES[582] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_582);
  NAMES[583] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_583);
  NAMES[584] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_584);
  NAMES[585] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_585);
  NAMES[586] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_586);
  NAMES[587] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_587);
  NAMES[588] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_588);
  NAMES[589] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_589);
  NAMES[590] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_590);
  NAMES[591] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_591);
  NAMES[592] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_592);
  NAMES[593] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_593);
  NAMES[594] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_594);
  NAMES[595] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_595);
  NAMES[596] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_596);
  NAMES[597] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_597);
  NAMES[598] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_598);
  NAMES[599] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_599);
  NAMES[600] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_600);
  NAMES[601] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_601);
  NAMES[602] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_602);
  NAMES[603] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_603);
  NAMES[604] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_604);
  NAMES[605] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_605);
  NAMES[606] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_606);
  NAMES[607] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_607);
  NAMES[608] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_608);
  NAMES[609] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_609);
  NAMES[610] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_610);
  NAMES[611] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_611);
  NAMES[612] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_612);
  NAMES[613] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_613);
  NAMES[614] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_614);
  NAMES[615] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_615);
  NAMES[616] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_616);
  NAMES[617] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_617);
  NAMES[618] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_618);
  NAMES[619] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_619);
  NAMES[620] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_620);
  NAMES[621] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_621);
  NAMES[622] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_622);
  NAMES[623] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_623);
  NAMES[624] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_624);
  NAMES[625] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_625);
  NAMES[626] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_626);
  NAMES[627] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_627);
  NAMES[628] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_628);
  NAMES[629] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_629);
  NAMES[630] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_630);
  NAMES[631] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_631);
  NAMES[632] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_632);
  NAMES[633] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_633);
  NAMES[634] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_634);
  NAMES[635] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_635);
  NAMES[636] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_636);
  NAMES[637] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_637);
  NAMES[638] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_638);
  NAMES[639] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_639);
  NAMES[640] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_640);
  NAMES[641] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_641);
  NAMES[642] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_642);
  NAMES[643] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_643);
  NAMES[644] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_644);
  NAMES[645] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_645);
  NAMES[646] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_646);
  NAMES[647] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_647);
  NAMES[648] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_648);
  NAMES[649] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_649);
  NAMES[650] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_650);
  NAMES[651] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_651);
  NAMES[652] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_652);
  NAMES[653] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_653);
  NAMES[654] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_654);
  NAMES[655] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_655);
  NAMES[656] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_656);
  NAMES[657] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_657);
  NAMES[658] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_658);
  NAMES[659] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_659);
  NAMES[660] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_660);
  NAMES[661] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_661);
  NAMES[662] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_662);
  NAMES[663] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_663);
  NAMES[664] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_664);
  NAMES[665] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_665);
  NAMES[666] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_666);
  NAMES[667] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_667);
  NAMES[668] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_668);
  NAMES[669] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_669);
  NAMES[670] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_670);
  NAMES[671] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_671);
  NAMES[672] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_672);
  NAMES[673] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_673);
  NAMES[674] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_674);
  NAMES[675] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_675);
  NAMES[676] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_676);
  NAMES[677] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_677);
  NAMES[678] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_678);
  NAMES[679] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_679);
  NAMES[680] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_680);
  NAMES[681] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_681);
  NAMES[682] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_682);
  NAMES[683] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_683);
  NAMES[684] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_684);
  NAMES[685] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_685);
  NAMES[686] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_686);
  NAMES[687] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_687);
  NAMES[688] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_688);
  NAMES[689] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_689);
  NAMES[690] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_690);
  NAMES[691] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_691);
  NAMES[692] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_692);
  NAMES[693] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_693);
  NAMES[694] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_694);
  NAMES[695] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_695);
  NAMES[696] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_696);
  NAMES[697] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_697);
  NAMES[698] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_698);
  NAMES[699] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_699);
  NAMES[700] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_700);
  NAMES[701] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_701);
  NAMES[702] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_702);
  NAMES[703] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_703);
  NAMES[704] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_704);
  NAMES[705] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_705);
  NAMES[706] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_706);
  NAMES[707] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_707);
  NAMES[708] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_708);
  NAMES[709] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_709);
  NAMES[710] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_710);
  NAMES[711] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_711);
  NAMES[712] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_712);
  NAMES[713] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_713);
  NAMES[714] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_714);
  NAMES[715] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_715);
  NAMES[716] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_716);
  NAMES[717] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_717);
  NAMES[718] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_718);
  NAMES[719] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_719);
  NAMES[720] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_720);
  NAMES[721] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_721);
  NAMES[722] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_722);
  NAMES[723] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_723);
  NAMES[724] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_724);
  NAMES[725] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_725);
  NAMES[726] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_726);
  NAMES[727] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_727);
  NAMES[728] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_728);
  NAMES[729] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_729);
  NAMES[730] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_730);
  NAMES[731] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_731);
  NAMES[732] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_732);
  NAMES[733] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_733);
  NAMES[734] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_734);
  NAMES[735] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_735);
  NAMES[736] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_736);
  NAMES[737] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_737);
  NAMES[738] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_738);
  NAMES[739] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_739);
  NAMES[740] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_740);
  NAMES[741] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_741);
  NAMES[742] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_742);
  NAMES[743] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_743);
  NAMES[744] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_744);
  NAMES[745] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_745);
  NAMES[746] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_746);
  NAMES[747] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_747);
  NAMES[748] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_748);
  NAMES[749] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_749);
  NAMES[750] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_750);
  NAMES[751] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_751);
  NAMES[752] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_752);
  NAMES[753] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_753);
  NAMES[754] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_754);
  NAMES[755] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_755);
  NAMES[756] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_756);
  NAMES[757] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_757);
  NAMES[758] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_758);
  NAMES[759] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_759);
  NAMES[760] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_760);
  NAMES[761] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_761);
  NAMES[762] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_762);
  NAMES[763] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_763);
  NAMES[764] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_764);
  NAMES[765] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_765);
  NAMES[766] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_766);
  NAMES[767] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_767);
  NAMES[768] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_768);
  NAMES[769] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_769);
  NAMES[770] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_770);
  NAMES[771] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_771);
  NAMES[772] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_772);
  NAMES[773] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_773);
  NAMES[774] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_774);
  NAMES[775] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_775);
  NAMES[776] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_776);
  NAMES[777] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_777);
  NAMES[778] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_778);
  NAMES[779] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_779);
  NAMES[780] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_780);
  NAMES[781] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_781);
  NAMES[782] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_782);
  NAMES[783] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_783);
  NAMES[784] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_784);
  NAMES[785] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_785);
  NAMES[786] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_786);
  NAMES[787] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_787);
  NAMES[788] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_788);
  NAMES[789] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_789);
  NAMES[790] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_790);
  NAMES[791] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_791);
  NAMES[792] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_792);
  NAMES[793] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_793);
  NAMES[794] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_794);
  NAMES[795] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_795);
  NAMES[796] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_796);
  NAMES[797] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_797);
  NAMES[798] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_798);
  NAMES[799] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_799);
  NAMES[800] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_800);
  NAMES[801] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_801);
  NAMES[802] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_802);
  NAMES[803] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_803);
  NAMES[804] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_804);
  NAMES[805] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_805);
  NAMES[806] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_806);
  NAMES[807] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_807);
  NAMES[808] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_808);
  NAMES[809] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_809);
  NAMES[810] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_810);
  NAMES[811] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_811);
  NAMES[812] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_812);
  NAMES[813] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_813);
  NAMES[814] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_814);
  NAMES[815] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_815);
  NAMES[816] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_816);
  NAMES[817] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_817);
  NAMES[818] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_818);
  NAMES[819] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_819);
  NAMES[820] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_820);
  NAMES[821] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_821);
  NAMES[822] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_822);
  NAMES[823] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_823);
  NAMES[824] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_824);
  NAMES[825] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_825);
  NAMES[826] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_826);
  NAMES[827] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_827);
  NAMES[828] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_828);
  NAMES[829] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_829);
  NAMES[830] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_830);
  NAMES[831] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_831);
  NAMES[832] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_832);
  NAMES[833] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_833);
  NAMES[834] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_834);
  NAMES[835] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_835);
  NAMES[836] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_836);
  NAMES[837] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_837);
  NAMES[838] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_838);
  NAMES[839] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_839);
  NAMES[840] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_840);
  NAMES[841] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_841);
  NAMES[842] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_842);
  NAMES[843] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_843);
  NAMES[844] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_844);
  NAMES[845] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_845);
  NAMES[846] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_846);
  NAMES[847] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_847);
  NAMES[848] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_848);
  NAMES[849] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_849);
  NAMES[850] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_850);
  NAMES[851] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_851);
  NAMES[852] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_852);
  NAMES[853] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_853);
  NAMES[854] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_854);
  NAMES[855] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_855);
  NAMES[856] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_856);
  NAMES[857] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_857);
  NAMES[858] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_858);
  NAMES[859] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_859);
  NAMES[860] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_860);
  NAMES[861] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_861);
  NAMES[862] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_862);
  NAMES[863] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_863);
  NAMES[864] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_864);
  NAMES[865] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_865);
  NAMES[866] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_866);
  NAMES[867] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_867);
  NAMES[868] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_868);
  NAMES[869] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_869);
  NAMES[870] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_870);
  NAMES[871] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_871);
  NAMES[872] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_872);
  NAMES[873] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_873);
  NAMES[874] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_874);
  NAMES[875] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_875);
  NAMES[876] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_876);
  NAMES[877] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_877);
  NAMES[878] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_878);
  NAMES[879] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_879);
  NAMES[880] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_880);
  NAMES[881] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_881);
  NAMES[882] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_882);
  NAMES[883] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_883);
  NAMES[884] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_884);
  NAMES[885] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_885);
  NAMES[886] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_886);
  NAMES[887] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_887);
  NAMES[888] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_888);
  NAMES[889] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_889);
  NAMES[890] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_890);
  NAMES[891] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_891);
  NAMES[892] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_892);
  NAMES[893] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_893);
  NAMES[894] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_894);
  NAMES[895] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_895);
  NAMES[896] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_896);
  NAMES[897] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_897);
  NAMES[898] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_898);
  NAMES[899] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_899);
  NAMES[900] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_900);
  NAMES[901] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_901);
  NAMES[902] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_902);
  NAMES[903] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_903);
  NAMES[904] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_904);
  NAMES[905] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_905);
  NAMES[906] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_906);
  NAMES[907] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_907);
  NAMES[908] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_908);
  NAMES[909] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_909);
  NAMES[910] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_910);
  NAMES[911] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_911);
  NAMES[912] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_912);
  NAMES[913] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_913);
  NAMES[914] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_914);
  NAMES[915] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_915);
  NAMES[916] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_916);
  NAMES[917] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_917);
  NAMES[918] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_918);
  NAMES[919] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_919);
  NAMES[920] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_920);
  NAMES[921] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_921);
  NAMES[922] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_922);
  NAMES[923] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_923);
  NAMES[924] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_924);
  NAMES[925] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_925);
  NAMES[926] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_926);
  NAMES[927] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_927);
  NAMES[928] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_928);
  NAMES[929] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_929);
  NAMES[930] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_930);
  NAMES[931] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_931);
  NAMES[932] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_932);
  NAMES[933] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_933);
  NAMES[934] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_934);
  NAMES[935] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_935);
  NAMES[936] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_936);
  NAMES[937] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_937);
  NAMES[938] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_938);
  NAMES[939] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_939);
  NAMES[940] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_940);
  NAMES[941] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_941);
  NAMES[942] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_942);
  NAMES[943] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_943);
  NAMES[944] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_944);
  NAMES[945] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_945);
  NAMES[946] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_946);
  NAMES[947] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_947);
  NAMES[948] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_948);
  NAMES[949] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_949);
  NAMES[950] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_950);
  NAMES[951] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_951);
  NAMES[952] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_952);
  NAMES[953] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_953);
  NAMES[954] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_954);
  NAMES[955] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_955);
  NAMES[956] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_956);
  NAMES[957] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_957);
  NAMES[958] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_958);
  NAMES[959] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_959);
  NAMES[960] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_960);
  NAMES[961] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_961);
  NAMES[962] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_962);
  NAMES[963] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_963);
  NAMES[964] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_964);
  NAMES[965] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_965);
  NAMES[966] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_966);
  NAMES[967] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_967);
  NAMES[968] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_968);
  NAMES[969] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_969);
  NAMES[970] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_970);
  NAMES[971] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_971);
  NAMES[972] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_972);
  NAMES[973] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_973);
  NAMES[974] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_974);
  NAMES[975] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_975);
  NAMES[976] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_976);
  NAMES[977] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_977);
  NAMES[978] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_978);
  NAMES[979] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_979);
  NAMES[980] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_980);
  NAMES[981] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_981);
  NAMES[982] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_982);
  NAMES[983] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_983);
  NAMES[984] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_984);
  NAMES[985] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_985);
  NAMES[986] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_986);
  NAMES[987] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_987);
  NAMES[988] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_988);
  NAMES[989] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_989);
  NAMES[990] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_990);
  NAMES[991] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_991);
  NAMES[992] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_992);
  NAMES[993] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_993);
  NAMES[994] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_994);
  NAMES[995] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_995);
  NAMES[996] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_996);
  NAMES[997] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_997);
  NAMES[998] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_998);
  NAMES[999] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_999);
  NAMES[1000] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1000);
  NAMES[1001] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1001);
  NAMES[1002] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1002);
  NAMES[1003] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1003);
  NAMES[1004] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1004);
  NAMES[1005] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1005);
  NAMES[1006] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1006);
  NAMES[1007] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1007);
  NAMES[1008] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1008);
  NAMES[1009] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1009);
  NAMES[1010] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1010);
  NAMES[1011] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1011);
  NAMES[1012] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1012);
  NAMES[1013] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1013);
  NAMES[1014] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1014);
  NAMES[1015] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1015);
  NAMES[1016] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1016);
  NAMES[1017] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1017);
  NAMES[1018] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1018);
  NAMES[1019] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1019);
  NAMES[1020] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1020);
  NAMES[1021] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1021);
  NAMES[1022] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1022);
  NAMES[1023] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1023);
  NAMES[1024] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1024);
  NAMES[1025] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1025);
  NAMES[1026] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1026);
  NAMES[1027] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1027);
  NAMES[1028] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1028);
  NAMES[1029] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1029);
  NAMES[1030] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1030);
  NAMES[1031] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1031);
  NAMES[1032] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1032);
  NAMES[1033] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1033);
  NAMES[1034] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1034);
  NAMES[1035] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1035);
  NAMES[1036] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1036);
  NAMES[1037] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1037);
  NAMES[1038] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1038);
  NAMES[1039] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1039);
  NAMES[1040] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1040);
  NAMES[1041] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1041);
  NAMES[1042] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1042);
  NAMES[1043] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1043);
  NAMES[1044] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1044);
  NAMES[1045] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1045);
  NAMES[1046] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1046);
  NAMES[1047] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1047);
  NAMES[1048] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1048);
  NAMES[1049] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1049);
  NAMES[1050] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1050);
  NAMES[1051] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1051);
  NAMES[1052] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1052);
  NAMES[1053] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1053);
  NAMES[1054] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1054);
  NAMES[1055] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1055);
  NAMES[1056] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1056);
  NAMES[1057] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1057);
  NAMES[1058] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1058);
  NAMES[1059] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1059);
  NAMES[1060] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1060);
  NAMES[1061] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1061);
  NAMES[1062] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1062);
  NAMES[1063] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1063);
  NAMES[1064] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1064);
  NAMES[1065] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1065);
  NAMES[1066] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1066);
  NAMES[1067] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1067);
  NAMES[1068] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1068);
  NAMES[1069] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1069);
  NAMES[1070] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1070);
  NAMES[1071] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1071);
  NAMES[1072] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1072);
  NAMES[1073] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1073);
  NAMES[1074] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1074);
  NAMES[1075] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1075);
  NAMES[1076] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1076);
  NAMES[1077] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1077);
  NAMES[1078] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1078);
  NAMES[1079] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1079);
  NAMES[1080] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1080);
  NAMES[1081] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1081);
  NAMES[1082] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1082);
  NAMES[1083] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1083);
  NAMES[1084] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1084);
  NAMES[1085] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1085);
  NAMES[1086] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1086);
  NAMES[1087] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1087);
  NAMES[1088] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1088);
  NAMES[1089] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1089);
  NAMES[1090] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1090);
  NAMES[1091] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1091);
  NAMES[1092] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1092);
  NAMES[1093] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1093);
  NAMES[1094] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1094);
  NAMES[1095] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1095);
  NAMES[1096] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1096);
  NAMES[1097] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1097);
  NAMES[1098] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1098);
  NAMES[1099] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1099);
  NAMES[1100] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1100);
  NAMES[1101] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1101);
  NAMES[1102] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1102);
  NAMES[1103] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1103);
  NAMES[1104] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1104);
  NAMES[1105] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1105);
  NAMES[1106] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1106);
  NAMES[1107] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1107);
  NAMES[1108] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1108);
  NAMES[1109] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1109);
  NAMES[1110] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1110);
  NAMES[1111] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1111);
  NAMES[1112] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1112);
  NAMES[1113] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1113);
  NAMES[1114] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1114);
  NAMES[1115] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1115);
  NAMES[1116] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1116);
  NAMES[1117] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1117);
  NAMES[1118] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1118);
  NAMES[1119] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1119);
  NAMES[1120] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1120);
  NAMES[1121] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1121);
  NAMES[1122] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1122);
  NAMES[1123] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1123);
  NAMES[1124] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1124);
  NAMES[1125] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1125);
  NAMES[1126] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1126);
  NAMES[1127] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1127);
  NAMES[1128] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1128);
  NAMES[1129] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1129);
  NAMES[1130] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1130);
  NAMES[1131] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1131);
  NAMES[1132] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1132);
  NAMES[1133] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1133);
  NAMES[1134] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1134);
  NAMES[1135] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1135);
  NAMES[1136] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1136);
  NAMES[1137] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1137);
  NAMES[1138] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1138);
  NAMES[1139] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1139);
  NAMES[1140] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1140);
  NAMES[1141] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1141);
  NAMES[1142] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1142);
  NAMES[1143] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1143);
  NAMES[1144] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1144);
  NAMES[1145] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1145);
  NAMES[1146] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1146);
  NAMES[1147] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1147);
  NAMES[1148] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1148);
  NAMES[1149] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1149);
  NAMES[1150] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1150);
  NAMES[1151] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1151);
  NAMES[1152] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1152);
  NAMES[1153] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1153);
  NAMES[1154] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1154);
  NAMES[1155] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1155);
  NAMES[1156] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1156);
  NAMES[1157] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1157);
  NAMES[1158] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1158);
  NAMES[1159] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1159);
  NAMES[1160] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1160);
  NAMES[1161] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1161);
  NAMES[1162] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1162);
  NAMES[1163] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1163);
  NAMES[1164] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1164);
  NAMES[1165] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1165);
  NAMES[1166] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1166);
  NAMES[1167] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1167);
  NAMES[1168] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1168);
  NAMES[1169] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1169);
  NAMES[1170] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1170);
  NAMES[1171] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1171);
  NAMES[1172] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1172);
  NAMES[1173] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1173);
  NAMES[1174] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1174);
  NAMES[1175] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1175);
  NAMES[1176] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1176);
  NAMES[1177] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1177);
  NAMES[1178] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1178);
  NAMES[1179] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1179);
  NAMES[1180] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1180);
  NAMES[1181] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1181);
  NAMES[1182] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1182);
  NAMES[1183] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1183);
  NAMES[1184] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1184);
  NAMES[1185] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1185);
  NAMES[1186] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1186);
  NAMES[1187] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1187);
  NAMES[1188] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1188);
  NAMES[1189] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1189);
  NAMES[1190] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1190);
  NAMES[1191] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1191);
  NAMES[1192] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1192);
  NAMES[1193] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1193);
  NAMES[1194] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1194);
  NAMES[1195] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1195);
  NAMES[1196] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1196);
  NAMES[1197] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1197);
  NAMES[1198] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1198);
  NAMES[1199] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1199);
  NAMES[1200] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1200);
  NAMES[1201] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1201);
  NAMES[1202] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1202);
  NAMES[1203] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1203);
  NAMES[1204] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1204);
  NAMES[1205] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1205);
  NAMES[1206] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1206);
  NAMES[1207] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1207);
  NAMES[1208] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1208);
  NAMES[1209] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1209);
  NAMES[1210] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1210);
  NAMES[1211] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1211);
  NAMES[1212] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1212);
  NAMES[1213] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1213);
  NAMES[1214] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1214);
  NAMES[1215] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1215);
  NAMES[1216] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1216);
  NAMES[1217] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1217);
  NAMES[1218] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1218);
  NAMES[1219] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1219);
  NAMES[1220] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1220);
  NAMES[1221] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1221);
  NAMES[1222] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1222);
  NAMES[1223] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1223);
  NAMES[1224] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1224);
  NAMES[1225] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1225);
  NAMES[1226] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1226);
  NAMES[1227] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1227);
  NAMES[1228] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1228);
  NAMES[1229] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1229);
  NAMES[1230] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1230);
  NAMES[1231] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1231);
  NAMES[1232] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1232);
  NAMES[1233] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1233);
  NAMES[1234] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1234);
  NAMES[1235] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1235);
  NAMES[1236] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1236);
  NAMES[1237] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1237);
  NAMES[1238] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1238);
  NAMES[1239] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1239);
  NAMES[1240] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1240);
  NAMES[1241] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1241);
  NAMES[1242] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1242);
  NAMES[1243] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1243);
  NAMES[1244] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1244);
  NAMES[1245] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1245);
  NAMES[1246] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1246);
  NAMES[1247] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1247);
  NAMES[1248] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1248);
  NAMES[1249] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1249);
  NAMES[1250] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1250);
  NAMES[1251] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1251);
  NAMES[1252] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1252);
  NAMES[1253] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1253);
  NAMES[1254] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1254);
  NAMES[1255] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1255);
  NAMES[1256] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1256);
  NAMES[1257] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1257);
  NAMES[1258] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1258);
  NAMES[1259] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1259);
  NAMES[1260] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1260);
  NAMES[1261] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1261);
  NAMES[1262] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1262);
  NAMES[1263] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1263);
  NAMES[1264] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1264);
  NAMES[1265] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1265);
  NAMES[1266] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1266);
  NAMES[1267] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1267);
  NAMES[1268] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1268);
  NAMES[1269] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1269);
  NAMES[1270] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1270);
  NAMES[1271] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1271);
  NAMES[1272] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1272);
  NAMES[1273] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1273);
  NAMES[1274] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1274);
  NAMES[1275] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1275);
  NAMES[1276] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1276);
  NAMES[1277] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1277);
  NAMES[1278] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1278);
  NAMES[1279] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1279);
  NAMES[1280] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1280);
  NAMES[1281] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1281);
  NAMES[1282] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1282);
  NAMES[1283] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1283);
  NAMES[1284] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1284);
  NAMES[1285] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1285);
  NAMES[1286] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1286);
  NAMES[1287] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1287);
  NAMES[1288] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1288);
  NAMES[1289] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1289);
  NAMES[1290] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1290);
  NAMES[1291] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1291);
  NAMES[1292] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1292);
  NAMES[1293] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1293);
  NAMES[1294] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1294);
  NAMES[1295] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1295);
  NAMES[1296] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1296);
  NAMES[1297] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1297);
  NAMES[1298] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1298);
  NAMES[1299] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1299);
  NAMES[1300] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1300);
  NAMES[1301] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1301);
  NAMES[1302] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1302);
  NAMES[1303] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1303);
  NAMES[1304] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1304);
  NAMES[1305] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1305);
  NAMES[1306] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1306);
  NAMES[1307] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1307);
  NAMES[1308] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1308);
  NAMES[1309] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1309);
  NAMES[1310] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1310);
  NAMES[1311] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1311);
  NAMES[1312] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1312);
  NAMES[1313] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1313);
  NAMES[1314] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1314);
  NAMES[1315] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1315);
  NAMES[1316] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1316);
  NAMES[1317] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1317);
  NAMES[1318] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1318);
  NAMES[1319] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1319);
  NAMES[1320] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1320);
  NAMES[1321] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1321);
  NAMES[1322] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1322);
  NAMES[1323] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1323);
  NAMES[1324] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1324);
  NAMES[1325] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1325);
  NAMES[1326] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1326);
  NAMES[1327] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1327);
  NAMES[1328] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1328);
  NAMES[1329] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1329);
  NAMES[1330] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1330);
  NAMES[1331] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1331);
  NAMES[1332] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1332);
  NAMES[1333] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1333);
  NAMES[1334] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1334);
  NAMES[1335] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1335);
  NAMES[1336] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1336);
  NAMES[1337] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1337);
  NAMES[1338] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1338);
  NAMES[1339] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1339);
  NAMES[1340] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1340);
  NAMES[1341] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1341);
  NAMES[1342] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1342);
  NAMES[1343] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1343);
  NAMES[1344] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1344);
  NAMES[1345] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1345);
  NAMES[1346] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1346);
  NAMES[1347] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1347);
  NAMES[1348] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1348);
  NAMES[1349] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1349);
  NAMES[1350] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1350);
  NAMES[1351] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1351);
  NAMES[1352] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1352);
  NAMES[1353] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1353);
  NAMES[1354] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1354);
  NAMES[1355] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1355);
  NAMES[1356] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1356);
  NAMES[1357] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1357);
  NAMES[1358] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1358);
  NAMES[1359] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1359);
  NAMES[1360] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1360);
  NAMES[1361] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1361);
  NAMES[1362] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1362);
  NAMES[1363] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1363);
  NAMES[1364] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1364);
  NAMES[1365] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1365);
  NAMES[1366] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1366);
  NAMES[1367] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1367);
  NAMES[1368] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1368);
  NAMES[1369] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1369);
  NAMES[1370] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1370);
  NAMES[1371] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1371);
  NAMES[1372] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1372);
  NAMES[1373] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1373);
  NAMES[1374] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1374);
  NAMES[1375] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1375);
  NAMES[1376] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1376);
  NAMES[1377] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1377);
  NAMES[1378] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1378);
  NAMES[1379] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1379);
  NAMES[1380] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1380);
  NAMES[1381] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1381);
  NAMES[1382] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1382);
  NAMES[1383] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1383);
  NAMES[1384] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1384);
  NAMES[1385] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1385);
  NAMES[1386] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1386);
  NAMES[1387] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1387);
  NAMES[1388] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1388);
  NAMES[1389] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1389);
  NAMES[1390] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1390);
  NAMES[1391] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1391);
  NAMES[1392] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1392);
  NAMES[1393] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1393);
  NAMES[1394] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1394);
  NAMES[1395] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1395);
  NAMES[1396] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1396);
  NAMES[1397] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1397);
  NAMES[1398] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1398);
  NAMES[1399] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1399);
  NAMES[1400] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1400);
  NAMES[1401] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1401);
  NAMES[1402] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1402);
  NAMES[1403] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1403);
  NAMES[1404] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1404);
  NAMES[1405] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1405);
  NAMES[1406] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1406);
  NAMES[1407] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1407);
  NAMES[1408] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1408);
  NAMES[1409] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1409);
  NAMES[1410] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1410);
  NAMES[1411] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1411);
  NAMES[1412] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1412);
  NAMES[1413] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1413);
  NAMES[1414] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1414);
  NAMES[1415] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1415);
  NAMES[1416] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1416);
  NAMES[1417] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1417);
  NAMES[1418] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1418);
  NAMES[1419] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1419);
  NAMES[1420] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1420);
  NAMES[1421] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1421);
  NAMES[1422] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1422);
  NAMES[1423] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1423);
  NAMES[1424] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1424);
  NAMES[1425] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1425);
  NAMES[1426] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1426);
  NAMES[1427] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1427);
  NAMES[1428] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1428);
  NAMES[1429] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1429);
  NAMES[1430] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1430);
  NAMES[1431] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1431);
  NAMES[1432] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1432);
  NAMES[1433] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1433);
  NAMES[1434] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1434);
  NAMES[1435] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1435);
  NAMES[1436] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1436);
  NAMES[1437] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1437);
  NAMES[1438] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1438);
  NAMES[1439] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1439);
  NAMES[1440] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1440);
  NAMES[1441] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1441);
  NAMES[1442] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1442);
  NAMES[1443] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1443);
  NAMES[1444] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1444);
  NAMES[1445] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1445);
  NAMES[1446] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1446);
  NAMES[1447] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1447);
  NAMES[1448] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1448);
  NAMES[1449] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1449);
  NAMES[1450] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1450);
  NAMES[1451] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1451);
  NAMES[1452] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1452);
  NAMES[1453] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1453);
  NAMES[1454] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1454);
  NAMES[1455] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1455);
  NAMES[1456] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1456);
  NAMES[1457] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1457);
  NAMES[1458] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1458);
  NAMES[1459] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1459);
  NAMES[1460] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1460);
  NAMES[1461] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1461);
  NAMES[1462] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1462);
  NAMES[1463] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1463);
  NAMES[1464] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1464);
  NAMES[1465] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1465);
  NAMES[1466] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1466);
  NAMES[1467] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1467);
  NAMES[1468] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1468);
  NAMES[1469] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1469);
  NAMES[1470] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1470);
  NAMES[1471] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1471);
  NAMES[1472] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1472);
  NAMES[1473] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1473);
  NAMES[1474] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1474);
  NAMES[1475] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1475);
  NAMES[1476] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1476);
  NAMES[1477] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1477);
  NAMES[1478] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1478);
  NAMES[1479] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1479);
  NAMES[1480] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1480);
  NAMES[1481] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1481);
  NAMES[1482] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1482);
  NAMES[1483] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1483);
  NAMES[1484] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1484);
  NAMES[1485] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1485);
  NAMES[1486] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1486);
  NAMES[1487] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1487);
  NAMES[1488] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1488);
  NAMES[1489] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1489);
  NAMES[1490] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1490);
  NAMES[1491] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1491);
  NAMES[1492] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1492);
  NAMES[1493] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1493);
  NAMES[1494] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1494);
  NAMES[1495] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1495);
  NAMES[1496] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1496);
  NAMES[1497] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1497);
  NAMES[1498] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1498);
  NAMES[1499] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1499);
  NAMES[1500] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1500);
  NAMES[1501] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1501);
  NAMES[1502] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1502);
  NAMES[1503] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1503);
  NAMES[1504] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1504);
  NAMES[1505] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1505);
  NAMES[1506] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1506);
  NAMES[1507] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1507);
  NAMES[1508] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1508);
  NAMES[1509] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1509);
  NAMES[1510] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1510);
  NAMES[1511] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1511);
  NAMES[1512] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1512);
  NAMES[1513] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1513);
  NAMES[1514] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1514);
  NAMES[1515] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1515);
  NAMES[1516] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1516);
  NAMES[1517] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1517);
  NAMES[1518] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1518);
  NAMES[1519] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1519);
  NAMES[1520] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1520);
  NAMES[1521] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1521);
  NAMES[1522] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1522);
  NAMES[1523] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1523);
  NAMES[1524] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1524);
  NAMES[1525] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1525);
  NAMES[1526] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1526);
  NAMES[1527] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1527);
  NAMES[1528] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1528);
  NAMES[1529] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1529);
  NAMES[1530] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1530);
  NAMES[1531] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1531);
  NAMES[1532] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1532);
  NAMES[1533] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1533);
  NAMES[1534] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1534);
  NAMES[1535] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1535);
  NAMES[1536] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1536);
  NAMES[1537] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1537);
  NAMES[1538] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1538);
  NAMES[1539] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1539);
  NAMES[1540] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1540);
  NAMES[1541] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1541);
  NAMES[1542] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1542);
  NAMES[1543] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1543);
  NAMES[1544] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1544);
  NAMES[1545] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1545);
  NAMES[1546] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1546);
  NAMES[1547] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1547);
  NAMES[1548] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1548);
  NAMES[1549] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1549);
  NAMES[1550] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1550);
  NAMES[1551] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1551);
  NAMES[1552] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1552);
  NAMES[1553] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1553);
  NAMES[1554] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1554);
  NAMES[1555] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1555);
  NAMES[1556] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1556);
  NAMES[1557] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1557);
  NAMES[1558] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1558);
  NAMES[1559] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1559);
  NAMES[1560] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1560);
  NAMES[1561] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1561);
  NAMES[1562] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1562);
  NAMES[1563] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1563);
  NAMES[1564] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1564);
  NAMES[1565] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1565);
  NAMES[1566] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1566);
  NAMES[1567] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1567);
  NAMES[1568] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1568);
  NAMES[1569] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1569);
  NAMES[1570] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1570);
  NAMES[1571] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1571);
  NAMES[1572] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1572);
  NAMES[1573] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1573);
  NAMES[1574] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1574);
  NAMES[1575] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1575);
  NAMES[1576] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1576);
  NAMES[1577] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1577);
  NAMES[1578] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1578);
  NAMES[1579] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1579);
  NAMES[1580] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1580);
  NAMES[1581] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1581);
  NAMES[1582] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1582);
  NAMES[1583] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1583);
  NAMES[1584] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1584);
  NAMES[1585] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1585);
  NAMES[1586] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1586);
  NAMES[1587] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1587);
  NAMES[1588] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1588);
  NAMES[1589] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1589);
  NAMES[1590] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1590);
  NAMES[1591] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1591);
  NAMES[1592] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1592);
  NAMES[1593] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1593);
  NAMES[1594] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1594);
  NAMES[1595] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1595);
  NAMES[1596] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1596);
  NAMES[1597] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1597);
  NAMES[1598] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1598);
  NAMES[1599] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1599);
  NAMES[1600] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1600);
  NAMES[1601] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1601);
  NAMES[1602] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1602);
  NAMES[1603] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1603);
  NAMES[1604] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1604);
  NAMES[1605] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1605);
  NAMES[1606] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1606);
  NAMES[1607] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1607);
  NAMES[1608] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1608);
  NAMES[1609] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1609);
  NAMES[1610] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1610);
  NAMES[1611] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1611);
  NAMES[1612] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1612);
  NAMES[1613] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1613);
  NAMES[1614] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1614);
  NAMES[1615] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1615);
  NAMES[1616] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1616);
  NAMES[1617] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1617);
  NAMES[1618] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1618);
  NAMES[1619] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1619);
  NAMES[1620] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1620);
  NAMES[1621] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1621);
  NAMES[1622] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1622);
  NAMES[1623] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1623);
  NAMES[1624] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1624);
  NAMES[1625] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1625);
  NAMES[1626] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1626);
  NAMES[1627] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1627);
  NAMES[1628] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1628);
  NAMES[1629] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1629);
  NAMES[1630] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1630);
  NAMES[1631] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1631);
  NAMES[1632] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1632);
  NAMES[1633] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1633);
  NAMES[1634] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1634);
  NAMES[1635] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1635);
  NAMES[1636] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1636);
  NAMES[1637] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1637);
  NAMES[1638] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1638);
  NAMES[1639] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1639);
  NAMES[1640] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1640);
  NAMES[1641] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1641);
  NAMES[1642] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1642);
  NAMES[1643] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1643);
  NAMES[1644] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1644);
  NAMES[1645] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1645);
  NAMES[1646] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1646);
  NAMES[1647] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1647);
  NAMES[1648] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1648);
  NAMES[1649] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1649);
  NAMES[1650] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1650);
  NAMES[1651] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1651);
  NAMES[1652] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1652);
  NAMES[1653] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1653);
  NAMES[1654] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1654);
  NAMES[1655] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1655);
  NAMES[1656] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1656);
  NAMES[1657] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1657);
  NAMES[1658] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1658);
  NAMES[1659] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1659);
  NAMES[1660] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1660);
  NAMES[1661] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1661);
  NAMES[1662] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1662);
  NAMES[1663] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1663);
  NAMES[1664] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1664);
  NAMES[1665] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1665);
  NAMES[1666] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1666);
  NAMES[1667] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1667);
  NAMES[1668] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1668);
  NAMES[1669] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1669);
  NAMES[1670] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1670);
  NAMES[1671] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1671);
  NAMES[1672] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1672);
  NAMES[1673] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1673);
  NAMES[1674] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1674);
  NAMES[1675] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1675);
  NAMES[1676] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1676);
  NAMES[1677] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1677);
  NAMES[1678] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1678);
  NAMES[1679] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1679);
  NAMES[1680] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1680);
  NAMES[1681] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1681);
  NAMES[1682] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1682);
  NAMES[1683] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1683);
  NAMES[1684] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1684);
  NAMES[1685] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1685);
  NAMES[1686] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1686);
  NAMES[1687] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1687);
  NAMES[1688] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1688);
  NAMES[1689] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1689);
  NAMES[1690] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1690);
  NAMES[1691] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1691);
  NAMES[1692] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1692);
  NAMES[1693] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1693);
  NAMES[1694] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1694);
  NAMES[1695] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1695);
  NAMES[1696] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1696);
  NAMES[1697] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1697);
  NAMES[1698] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1698);
  NAMES[1699] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1699);
  NAMES[1700] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1700);
  NAMES[1701] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1701);
  NAMES[1702] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1702);
  NAMES[1703] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1703);
  NAMES[1704] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1704);
  NAMES[1705] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1705);
  NAMES[1706] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1706);
  NAMES[1707] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1707);
  NAMES[1708] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1708);
  NAMES[1709] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1709);
  NAMES[1710] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1710);
  NAMES[1711] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1711);
  NAMES[1712] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1712);
  NAMES[1713] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1713);
  NAMES[1714] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1714);
  NAMES[1715] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1715);
  NAMES[1716] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1716);
  NAMES[1717] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1717);
  NAMES[1718] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1718);
  NAMES[1719] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1719);
  NAMES[1720] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1720);
  NAMES[1721] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1721);
  NAMES[1722] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1722);
  NAMES[1723] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1723);
  NAMES[1724] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1724);
  NAMES[1725] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1725);
  NAMES[1726] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1726);
  NAMES[1727] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1727);
  NAMES[1728] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1728);
  NAMES[1729] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1729);
  NAMES[1730] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1730);
  NAMES[1731] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1731);
  NAMES[1732] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1732);
  NAMES[1733] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1733);
  NAMES[1734] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1734);
  NAMES[1735] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1735);
  NAMES[1736] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1736);
  NAMES[1737] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1737);
  NAMES[1738] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1738);
  NAMES[1739] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1739);
  NAMES[1740] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1740);
  NAMES[1741] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1741);
  NAMES[1742] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1742);
  NAMES[1743] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1743);
  NAMES[1744] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1744);
  NAMES[1745] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1745);
  NAMES[1746] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1746);
  NAMES[1747] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1747);
  NAMES[1748] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1748);
  NAMES[1749] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1749);
  NAMES[1750] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1750);
  NAMES[1751] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1751);
  NAMES[1752] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1752);
  NAMES[1753] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1753);
  NAMES[1754] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1754);
  NAMES[1755] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1755);
  NAMES[1756] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1756);
  NAMES[1757] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1757);
  NAMES[1758] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1758);
  NAMES[1759] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1759);
  NAMES[1760] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1760);
  NAMES[1761] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1761);
  NAMES[1762] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1762);
  NAMES[1763] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1763);
  NAMES[1764] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1764);
  NAMES[1765] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1765);
  NAMES[1766] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1766);
  NAMES[1767] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1767);
  NAMES[1768] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1768);
  NAMES[1769] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1769);
  NAMES[1770] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1770);
  NAMES[1771] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1771);
  NAMES[1772] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1772);
  NAMES[1773] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1773);
  NAMES[1774] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1774);
  NAMES[1775] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1775);
  NAMES[1776] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1776);
  NAMES[1777] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1777);
  NAMES[1778] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1778);
  NAMES[1779] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1779);
  NAMES[1780] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1780);
  NAMES[1781] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1781);
  NAMES[1782] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1782);
  NAMES[1783] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1783);
  NAMES[1784] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1784);
  NAMES[1785] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1785);
  NAMES[1786] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1786);
  NAMES[1787] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1787);
  NAMES[1788] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1788);
  NAMES[1789] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1789);
  NAMES[1790] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1790);
  NAMES[1791] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1791);
  NAMES[1792] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1792);
  NAMES[1793] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1793);
  NAMES[1794] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1794);
  NAMES[1795] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1795);
  NAMES[1796] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1796);
  NAMES[1797] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1797);
  NAMES[1798] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1798);
  NAMES[1799] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1799);
  NAMES[1800] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1800);
  NAMES[1801] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1801);
  NAMES[1802] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1802);
  NAMES[1803] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1803);
  NAMES[1804] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1804);
  NAMES[1805] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1805);
  NAMES[1806] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1806);
  NAMES[1807] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1807);
  NAMES[1808] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1808);
  NAMES[1809] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1809);
  NAMES[1810] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1810);
  NAMES[1811] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1811);
  NAMES[1812] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1812);
  NAMES[1813] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1813);
  NAMES[1814] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1814);
  NAMES[1815] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1815);
  NAMES[1816] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1816);
  NAMES[1817] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1817);
  NAMES[1818] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1818);
  NAMES[1819] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1819);
  NAMES[1820] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1820);
  NAMES[1821] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1821);
  NAMES[1822] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1822);
  NAMES[1823] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1823);
  NAMES[1824] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1824);
  NAMES[1825] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1825);
  NAMES[1826] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1826);
  NAMES[1827] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1827);
  NAMES[1828] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1828);
  NAMES[1829] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1829);
  NAMES[1830] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1830);
  NAMES[1831] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1831);
  NAMES[1832] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1832);
  NAMES[1833] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1833);
  NAMES[1834] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1834);
  NAMES[1835] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1835);
  NAMES[1836] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1836);
  NAMES[1837] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1837);
  NAMES[1838] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1838);
  NAMES[1839] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1839);
  NAMES[1840] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1840);
  NAMES[1841] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1841);
  NAMES[1842] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1842);
  NAMES[1843] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1843);
  NAMES[1844] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1844);
  NAMES[1845] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1845);
  NAMES[1846] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1846);
  NAMES[1847] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1847);
  NAMES[1848] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1848);
  NAMES[1849] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1849);
  NAMES[1850] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1850);
  NAMES[1851] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1851);
  NAMES[1852] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1852);
  NAMES[1853] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1853);
  NAMES[1854] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1854);
  NAMES[1855] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1855);
  NAMES[1856] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1856);
  NAMES[1857] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1857);
  NAMES[1858] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1858);
  NAMES[1859] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1859);
  NAMES[1860] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1860);
  NAMES[1861] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1861);
  NAMES[1862] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1862);
  NAMES[1863] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1863);
  NAMES[1864] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1864);
  NAMES[1865] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1865);
  NAMES[1866] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1866);
  NAMES[1867] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1867);
  NAMES[1868] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1868);
  NAMES[1869] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1869);
  NAMES[1870] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1870);
  NAMES[1871] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1871);
  NAMES[1872] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1872);
  NAMES[1873] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1873);
  NAMES[1874] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1874);
  NAMES[1875] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1875);
  NAMES[1876] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1876);
  NAMES[1877] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1877);
  NAMES[1878] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1878);
  NAMES[1879] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1879);
  NAMES[1880] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1880);
  NAMES[1881] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1881);
  NAMES[1882] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1882);
  NAMES[1883] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1883);
  NAMES[1884] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1884);
  NAMES[1885] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1885);
  NAMES[1886] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1886);
  NAMES[1887] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1887);
  NAMES[1888] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1888);
  NAMES[1889] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1889);
  NAMES[1890] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1890);
  NAMES[1891] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1891);
  NAMES[1892] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1892);
  NAMES[1893] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1893);
  NAMES[1894] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1894);
  NAMES[1895] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1895);
  NAMES[1896] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1896);
  NAMES[1897] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1897);
  NAMES[1898] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1898);
  NAMES[1899] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1899);
  NAMES[1900] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1900);
  NAMES[1901] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1901);
  NAMES[1902] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1902);
  NAMES[1903] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1903);
  NAMES[1904] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1904);
  NAMES[1905] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1905);
  NAMES[1906] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1906);
  NAMES[1907] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1907);
  NAMES[1908] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1908);
  NAMES[1909] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1909);
  NAMES[1910] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1910);
  NAMES[1911] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1911);
  NAMES[1912] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1912);
  NAMES[1913] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1913);
  NAMES[1914] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1914);
  NAMES[1915] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1915);
  NAMES[1916] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1916);
  NAMES[1917] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1917);
  NAMES[1918] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1918);
  NAMES[1919] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1919);
  NAMES[1920] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1920);
  NAMES[1921] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1921);
  NAMES[1922] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1922);
  NAMES[1923] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1923);
  NAMES[1924] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1924);
  NAMES[1925] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1925);
  NAMES[1926] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1926);
  NAMES[1927] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1927);
  NAMES[1928] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1928);
  NAMES[1929] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1929);
  NAMES[1930] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1930);
  NAMES[1931] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1931);
  NAMES[1932] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1932);
  NAMES[1933] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1933);
  NAMES[1934] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1934);
  NAMES[1935] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1935);
  NAMES[1936] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1936);
  NAMES[1937] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1937);
  NAMES[1938] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1938);
  NAMES[1939] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1939);
  NAMES[1940] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1940);
  NAMES[1941] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1941);
  NAMES[1942] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1942);
  NAMES[1943] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1943);
  NAMES[1944] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1944);
  NAMES[1945] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1945);
  NAMES[1946] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1946);
  NAMES[1947] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1947);
  NAMES[1948] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1948);
  NAMES[1949] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1949);
  NAMES[1950] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1950);
  NAMES[1951] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1951);
  NAMES[1952] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1952);
  NAMES[1953] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1953);
  NAMES[1954] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1954);
  NAMES[1955] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1955);
  NAMES[1956] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1956);
  NAMES[1957] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1957);
  NAMES[1958] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1958);
  NAMES[1959] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1959);
  NAMES[1960] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1960);
  NAMES[1961] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1961);
  NAMES[1962] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1962);
  NAMES[1963] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1963);
  NAMES[1964] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1964);
  NAMES[1965] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1965);
  NAMES[1966] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1966);
  NAMES[1967] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1967);
  NAMES[1968] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1968);
  NAMES[1969] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1969);
  NAMES[1970] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1970);
  NAMES[1971] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1971);
  NAMES[1972] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1972);
  NAMES[1973] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1973);
  NAMES[1974] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1974);
  NAMES[1975] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1975);
  NAMES[1976] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1976);
  NAMES[1977] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1977);
  NAMES[1978] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1978);
  NAMES[1979] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1979);
  NAMES[1980] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1980);
  NAMES[1981] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1981);
  NAMES[1982] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1982);
  NAMES[1983] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1983);
  NAMES[1984] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1984);
  NAMES[1985] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1985);
  NAMES[1986] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1986);
  NAMES[1987] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1987);
  NAMES[1988] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1988);
  NAMES[1989] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1989);
  NAMES[1990] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1990);
  NAMES[1991] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1991);
  NAMES[1992] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1992);
  NAMES[1993] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1993);
  NAMES[1994] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1994);
  NAMES[1995] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1995);
  NAMES[1996] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1996);
  NAMES[1997] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1997);
  NAMES[1998] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1998);
  NAMES[1999] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_1999);
  NAMES[2000] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2000);
  NAMES[2001] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2001);
  NAMES[2002] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2002);
  NAMES[2003] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2003);
  NAMES[2004] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2004);
  NAMES[2005] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2005);
  NAMES[2006] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2006);
  NAMES[2007] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2007);
  NAMES[2008] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2008);
  NAMES[2009] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2009);
  NAMES[2010] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2010);
  NAMES[2011] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2011);
  NAMES[2012] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2012);
  NAMES[2013] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2013);
  NAMES[2014] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2014);
  NAMES[2015] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2015);
  NAMES[2016] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2016);
  NAMES[2017] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2017);
  NAMES[2018] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2018);
  NAMES[2019] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2019);
  NAMES[2020] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2020);
  NAMES[2021] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2021);
  NAMES[2022] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2022);
  NAMES[2023] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2023);
  NAMES[2024] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2024);
  NAMES[2025] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2025);
  NAMES[2026] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2026);
  NAMES[2027] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2027);
  NAMES[2028] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2028);
  NAMES[2029] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2029);
  NAMES[2030] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2030);
  NAMES[2031] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2031);
  NAMES[2032] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2032);
  NAMES[2033] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2033);
  NAMES[2034] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2034);
  NAMES[2035] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2035);
  NAMES[2036] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2036);
  NAMES[2037] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2037);
  NAMES[2038] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2038);
  NAMES[2039] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2039);
  NAMES[2040] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2040);
  NAMES[2041] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2041);
  NAMES[2042] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2042);
  NAMES[2043] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2043);
  NAMES[2044] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2044);
  NAMES[2045] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2045);
  NAMES[2046] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2046);
  NAMES[2047] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2047);
  NAMES[2048] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2048);
  NAMES[2049] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2049);
  NAMES[2050] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2050);
  NAMES[2051] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2051);
  NAMES[2052] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2052);
  NAMES[2053] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2053);
  NAMES[2054] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2054);
  NAMES[2055] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2055);
  NAMES[2056] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2056);
  NAMES[2057] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2057);
  NAMES[2058] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2058);
  NAMES[2059] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2059);
  NAMES[2060] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2060);
  NAMES[2061] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2061);
  NAMES[2062] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2062);
  NAMES[2063] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2063);
  NAMES[2064] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2064);
  NAMES[2065] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2065);
  NAMES[2066] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2066);
  NAMES[2067] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2067);
  NAMES[2068] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2068);
  NAMES[2069] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2069);
  NAMES[2070] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2070);
  NAMES[2071] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2071);
  NAMES[2072] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2072);
  NAMES[2073] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2073);
  NAMES[2074] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2074);
  NAMES[2075] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2075);
  NAMES[2076] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2076);
  NAMES[2077] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2077);
  NAMES[2078] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2078);
  NAMES[2079] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2079);
  NAMES[2080] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2080);
  NAMES[2081] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2081);
  NAMES[2082] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2082);
  NAMES[2083] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2083);
  NAMES[2084] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2084);
  NAMES[2085] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2085);
  NAMES[2086] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2086);
  NAMES[2087] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2087);
  NAMES[2088] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2088);
  NAMES[2089] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2089);
  NAMES[2090] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2090);
  NAMES[2091] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2091);
  NAMES[2092] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2092);
  NAMES[2093] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2093);
  NAMES[2094] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2094);
  NAMES[2095] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2095);
  NAMES[2096] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2096);
  NAMES[2097] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2097);
  NAMES[2098] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2098);
  NAMES[2099] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2099);
  NAMES[2100] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2100);
  NAMES[2101] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2101);
  NAMES[2102] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2102);
  NAMES[2103] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2103);
  NAMES[2104] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2104);
  NAMES[2105] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2105);
  NAMES[2106] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2106);
  NAMES[2107] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2107);
  NAMES[2108] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2108);
  NAMES[2109] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2109);
  NAMES[2110] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2110);
  NAMES[2111] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2111);
  NAMES[2112] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2112);
  NAMES[2113] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2113);
  NAMES[2114] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2114);
  NAMES[2115] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2115);
  NAMES[2116] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2116);
  NAMES[2117] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2117);
  NAMES[2118] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2118);
  NAMES[2119] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2119);
  NAMES[2120] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2120);
  NAMES[2121] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2121);
  NAMES[2122] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2122);
  NAMES[2123] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2123);
  NAMES[2124] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2124);
  NAMES[2125] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2125);
  NAMES[2126] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2126);
  NAMES[2127] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2127);
  NAMES[2128] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2128);
  NAMES[2129] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2129);
  NAMES[2130] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2130);
  NAMES[2131] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2131);
  NAMES[2132] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2132);
  NAMES[2133] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2133);
  NAMES[2134] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2134);
  NAMES[2135] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2135);
  NAMES[2136] = J_ARRAY_STATIC(PRUnichar, PRInt32, NAME_2136);
  VALUES = new jArray<PRUnichar,PRInt32>[2137];
  VALUES[0] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_0);
  VALUES[1] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1);
  VALUES[2] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2);
  VALUES[3] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_3);
  VALUES[4] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_4);
  VALUES[5] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_5);
  VALUES[6] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_6);
  VALUES[7] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_7);
  VALUES[8] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_8);
  VALUES[9] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_9);
  VALUES[10] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_10);
  VALUES[11] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_11);
  VALUES[12] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_12);
  VALUES[13] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_13);
  VALUES[14] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_14);
  VALUES[15] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_15);
  VALUES[16] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_16);
  VALUES[17] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_17);
  VALUES[18] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_18);
  VALUES[19] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_19);
  VALUES[20] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_20);
  VALUES[21] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_21);
  VALUES[22] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_22);
  VALUES[23] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_23);
  VALUES[24] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_24);
  VALUES[25] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_25);
  VALUES[26] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_26);
  VALUES[27] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_27);
  VALUES[28] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_28);
  VALUES[29] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_29);
  VALUES[30] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_30);
  VALUES[31] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_31);
  VALUES[32] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_32);
  VALUES[33] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_33);
  VALUES[34] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_34);
  VALUES[35] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_35);
  VALUES[36] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_36);
  VALUES[37] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_37);
  VALUES[38] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_38);
  VALUES[39] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_39);
  VALUES[40] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_40);
  VALUES[41] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_41);
  VALUES[42] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_42);
  VALUES[43] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_43);
  VALUES[44] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_44);
  VALUES[45] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_45);
  VALUES[46] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_46);
  VALUES[47] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_47);
  VALUES[48] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_48);
  VALUES[49] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_49);
  VALUES[50] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_50);
  VALUES[51] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_51);
  VALUES[52] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_52);
  VALUES[53] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_53);
  VALUES[54] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_54);
  VALUES[55] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_55);
  VALUES[56] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_56);
  VALUES[57] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_57);
  VALUES[58] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_58);
  VALUES[59] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_59);
  VALUES[60] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_60);
  VALUES[61] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_61);
  VALUES[62] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_62);
  VALUES[63] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_63);
  VALUES[64] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_64);
  VALUES[65] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_65);
  VALUES[66] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_66);
  VALUES[67] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_67);
  VALUES[68] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_68);
  VALUES[69] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_69);
  VALUES[70] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_70);
  VALUES[71] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_71);
  VALUES[72] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_72);
  VALUES[73] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_73);
  VALUES[74] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_74);
  VALUES[75] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_75);
  VALUES[76] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_76);
  VALUES[77] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_77);
  VALUES[78] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_78);
  VALUES[79] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_79);
  VALUES[80] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_80);
  VALUES[81] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_81);
  VALUES[82] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_82);
  VALUES[83] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_83);
  VALUES[84] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_84);
  VALUES[85] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_85);
  VALUES[86] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_86);
  VALUES[87] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_87);
  VALUES[88] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_88);
  VALUES[89] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_89);
  VALUES[90] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_90);
  VALUES[91] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_91);
  VALUES[92] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_92);
  VALUES[93] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_93);
  VALUES[94] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_94);
  VALUES[95] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_95);
  VALUES[96] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_96);
  VALUES[97] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_97);
  VALUES[98] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_98);
  VALUES[99] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_99);
  VALUES[100] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_100);
  VALUES[101] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_101);
  VALUES[102] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_102);
  VALUES[103] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_103);
  VALUES[104] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_104);
  VALUES[105] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_105);
  VALUES[106] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_106);
  VALUES[107] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_107);
  VALUES[108] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_108);
  VALUES[109] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_109);
  VALUES[110] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_110);
  VALUES[111] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_111);
  VALUES[112] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_112);
  VALUES[113] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_113);
  VALUES[114] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_114);
  VALUES[115] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_115);
  VALUES[116] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_116);
  VALUES[117] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_117);
  VALUES[118] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_118);
  VALUES[119] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_119);
  VALUES[120] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_120);
  VALUES[121] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_121);
  VALUES[122] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_122);
  VALUES[123] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_123);
  VALUES[124] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_124);
  VALUES[125] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_125);
  VALUES[126] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_126);
  VALUES[127] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_127);
  VALUES[128] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_128);
  VALUES[129] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_129);
  VALUES[130] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_130);
  VALUES[131] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_131);
  VALUES[132] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_132);
  VALUES[133] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_133);
  VALUES[134] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_134);
  VALUES[135] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_135);
  VALUES[136] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_136);
  VALUES[137] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_137);
  VALUES[138] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_138);
  VALUES[139] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_139);
  VALUES[140] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_140);
  VALUES[141] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_141);
  VALUES[142] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_142);
  VALUES[143] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_143);
  VALUES[144] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_144);
  VALUES[145] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_145);
  VALUES[146] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_146);
  VALUES[147] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_147);
  VALUES[148] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_148);
  VALUES[149] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_149);
  VALUES[150] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_150);
  VALUES[151] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_151);
  VALUES[152] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_152);
  VALUES[153] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_153);
  VALUES[154] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_154);
  VALUES[155] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_155);
  VALUES[156] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_156);
  VALUES[157] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_157);
  VALUES[158] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_158);
  VALUES[159] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_159);
  VALUES[160] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_160);
  VALUES[161] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_161);
  VALUES[162] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_162);
  VALUES[163] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_163);
  VALUES[164] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_164);
  VALUES[165] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_165);
  VALUES[166] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_166);
  VALUES[167] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_167);
  VALUES[168] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_168);
  VALUES[169] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_169);
  VALUES[170] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_170);
  VALUES[171] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_171);
  VALUES[172] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_172);
  VALUES[173] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_173);
  VALUES[174] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_174);
  VALUES[175] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_175);
  VALUES[176] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_176);
  VALUES[177] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_177);
  VALUES[178] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_178);
  VALUES[179] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_179);
  VALUES[180] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_180);
  VALUES[181] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_181);
  VALUES[182] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_182);
  VALUES[183] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_183);
  VALUES[184] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_184);
  VALUES[185] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_185);
  VALUES[186] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_186);
  VALUES[187] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_187);
  VALUES[188] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_188);
  VALUES[189] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_189);
  VALUES[190] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_190);
  VALUES[191] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_191);
  VALUES[192] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_192);
  VALUES[193] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_193);
  VALUES[194] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_194);
  VALUES[195] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_195);
  VALUES[196] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_196);
  VALUES[197] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_197);
  VALUES[198] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_198);
  VALUES[199] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_199);
  VALUES[200] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_200);
  VALUES[201] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_201);
  VALUES[202] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_202);
  VALUES[203] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_203);
  VALUES[204] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_204);
  VALUES[205] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_205);
  VALUES[206] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_206);
  VALUES[207] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_207);
  VALUES[208] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_208);
  VALUES[209] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_209);
  VALUES[210] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_210);
  VALUES[211] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_211);
  VALUES[212] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_212);
  VALUES[213] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_213);
  VALUES[214] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_214);
  VALUES[215] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_215);
  VALUES[216] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_216);
  VALUES[217] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_217);
  VALUES[218] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_218);
  VALUES[219] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_219);
  VALUES[220] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_220);
  VALUES[221] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_221);
  VALUES[222] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_222);
  VALUES[223] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_223);
  VALUES[224] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_224);
  VALUES[225] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_225);
  VALUES[226] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_226);
  VALUES[227] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_227);
  VALUES[228] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_228);
  VALUES[229] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_229);
  VALUES[230] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_230);
  VALUES[231] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_231);
  VALUES[232] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_232);
  VALUES[233] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_233);
  VALUES[234] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_234);
  VALUES[235] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_235);
  VALUES[236] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_236);
  VALUES[237] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_237);
  VALUES[238] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_238);
  VALUES[239] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_239);
  VALUES[240] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_240);
  VALUES[241] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_241);
  VALUES[242] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_242);
  VALUES[243] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_243);
  VALUES[244] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_244);
  VALUES[245] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_245);
  VALUES[246] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_246);
  VALUES[247] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_247);
  VALUES[248] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_248);
  VALUES[249] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_249);
  VALUES[250] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_250);
  VALUES[251] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_251);
  VALUES[252] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_252);
  VALUES[253] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_253);
  VALUES[254] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_254);
  VALUES[255] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_255);
  VALUES[256] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_256);
  VALUES[257] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_257);
  VALUES[258] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_258);
  VALUES[259] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_259);
  VALUES[260] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_260);
  VALUES[261] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_261);
  VALUES[262] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_262);
  VALUES[263] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_263);
  VALUES[264] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_264);
  VALUES[265] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_265);
  VALUES[266] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_266);
  VALUES[267] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_267);
  VALUES[268] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_268);
  VALUES[269] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_269);
  VALUES[270] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_270);
  VALUES[271] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_271);
  VALUES[272] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_272);
  VALUES[273] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_273);
  VALUES[274] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_274);
  VALUES[275] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_275);
  VALUES[276] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_276);
  VALUES[277] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_277);
  VALUES[278] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_278);
  VALUES[279] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_279);
  VALUES[280] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_280);
  VALUES[281] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_281);
  VALUES[282] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_282);
  VALUES[283] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_283);
  VALUES[284] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_284);
  VALUES[285] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_285);
  VALUES[286] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_286);
  VALUES[287] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_287);
  VALUES[288] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_288);
  VALUES[289] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_289);
  VALUES[290] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_290);
  VALUES[291] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_291);
  VALUES[292] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_292);
  VALUES[293] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_293);
  VALUES[294] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_294);
  VALUES[295] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_295);
  VALUES[296] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_296);
  VALUES[297] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_297);
  VALUES[298] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_298);
  VALUES[299] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_299);
  VALUES[300] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_300);
  VALUES[301] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_301);
  VALUES[302] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_302);
  VALUES[303] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_303);
  VALUES[304] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_304);
  VALUES[305] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_305);
  VALUES[306] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_306);
  VALUES[307] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_307);
  VALUES[308] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_308);
  VALUES[309] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_309);
  VALUES[310] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_310);
  VALUES[311] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_311);
  VALUES[312] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_312);
  VALUES[313] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_313);
  VALUES[314] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_314);
  VALUES[315] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_315);
  VALUES[316] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_316);
  VALUES[317] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_317);
  VALUES[318] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_318);
  VALUES[319] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_319);
  VALUES[320] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_320);
  VALUES[321] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_321);
  VALUES[322] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_322);
  VALUES[323] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_323);
  VALUES[324] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_324);
  VALUES[325] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_325);
  VALUES[326] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_326);
  VALUES[327] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_327);
  VALUES[328] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_328);
  VALUES[329] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_329);
  VALUES[330] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_330);
  VALUES[331] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_331);
  VALUES[332] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_332);
  VALUES[333] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_333);
  VALUES[334] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_334);
  VALUES[335] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_335);
  VALUES[336] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_336);
  VALUES[337] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_337);
  VALUES[338] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_338);
  VALUES[339] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_339);
  VALUES[340] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_340);
  VALUES[341] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_341);
  VALUES[342] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_342);
  VALUES[343] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_343);
  VALUES[344] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_344);
  VALUES[345] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_345);
  VALUES[346] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_346);
  VALUES[347] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_347);
  VALUES[348] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_348);
  VALUES[349] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_349);
  VALUES[350] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_350);
  VALUES[351] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_351);
  VALUES[352] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_352);
  VALUES[353] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_353);
  VALUES[354] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_354);
  VALUES[355] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_355);
  VALUES[356] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_356);
  VALUES[357] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_357);
  VALUES[358] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_358);
  VALUES[359] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_359);
  VALUES[360] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_360);
  VALUES[361] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_361);
  VALUES[362] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_362);
  VALUES[363] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_363);
  VALUES[364] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_364);
  VALUES[365] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_365);
  VALUES[366] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_366);
  VALUES[367] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_367);
  VALUES[368] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_368);
  VALUES[369] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_369);
  VALUES[370] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_370);
  VALUES[371] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_371);
  VALUES[372] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_372);
  VALUES[373] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_373);
  VALUES[374] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_374);
  VALUES[375] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_375);
  VALUES[376] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_376);
  VALUES[377] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_377);
  VALUES[378] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_378);
  VALUES[379] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_379);
  VALUES[380] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_380);
  VALUES[381] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_381);
  VALUES[382] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_382);
  VALUES[383] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_383);
  VALUES[384] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_384);
  VALUES[385] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_385);
  VALUES[386] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_386);
  VALUES[387] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_387);
  VALUES[388] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_388);
  VALUES[389] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_389);
  VALUES[390] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_390);
  VALUES[391] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_391);
  VALUES[392] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_392);
  VALUES[393] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_393);
  VALUES[394] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_394);
  VALUES[395] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_395);
  VALUES[396] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_396);
  VALUES[397] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_397);
  VALUES[398] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_398);
  VALUES[399] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_399);
  VALUES[400] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_400);
  VALUES[401] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_401);
  VALUES[402] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_402);
  VALUES[403] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_403);
  VALUES[404] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_404);
  VALUES[405] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_405);
  VALUES[406] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_406);
  VALUES[407] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_407);
  VALUES[408] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_408);
  VALUES[409] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_409);
  VALUES[410] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_410);
  VALUES[411] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_411);
  VALUES[412] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_412);
  VALUES[413] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_413);
  VALUES[414] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_414);
  VALUES[415] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_415);
  VALUES[416] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_416);
  VALUES[417] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_417);
  VALUES[418] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_418);
  VALUES[419] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_419);
  VALUES[420] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_420);
  VALUES[421] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_421);
  VALUES[422] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_422);
  VALUES[423] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_423);
  VALUES[424] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_424);
  VALUES[425] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_425);
  VALUES[426] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_426);
  VALUES[427] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_427);
  VALUES[428] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_428);
  VALUES[429] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_429);
  VALUES[430] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_430);
  VALUES[431] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_431);
  VALUES[432] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_432);
  VALUES[433] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_433);
  VALUES[434] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_434);
  VALUES[435] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_435);
  VALUES[436] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_436);
  VALUES[437] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_437);
  VALUES[438] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_438);
  VALUES[439] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_439);
  VALUES[440] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_440);
  VALUES[441] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_441);
  VALUES[442] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_442);
  VALUES[443] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_443);
  VALUES[444] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_444);
  VALUES[445] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_445);
  VALUES[446] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_446);
  VALUES[447] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_447);
  VALUES[448] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_448);
  VALUES[449] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_449);
  VALUES[450] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_450);
  VALUES[451] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_451);
  VALUES[452] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_452);
  VALUES[453] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_453);
  VALUES[454] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_454);
  VALUES[455] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_455);
  VALUES[456] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_456);
  VALUES[457] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_457);
  VALUES[458] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_458);
  VALUES[459] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_459);
  VALUES[460] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_460);
  VALUES[461] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_461);
  VALUES[462] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_462);
  VALUES[463] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_463);
  VALUES[464] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_464);
  VALUES[465] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_465);
  VALUES[466] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_466);
  VALUES[467] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_467);
  VALUES[468] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_468);
  VALUES[469] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_469);
  VALUES[470] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_470);
  VALUES[471] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_471);
  VALUES[472] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_472);
  VALUES[473] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_473);
  VALUES[474] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_474);
  VALUES[475] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_475);
  VALUES[476] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_476);
  VALUES[477] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_477);
  VALUES[478] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_478);
  VALUES[479] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_479);
  VALUES[480] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_480);
  VALUES[481] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_481);
  VALUES[482] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_482);
  VALUES[483] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_483);
  VALUES[484] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_484);
  VALUES[485] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_485);
  VALUES[486] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_486);
  VALUES[487] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_487);
  VALUES[488] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_488);
  VALUES[489] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_489);
  VALUES[490] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_490);
  VALUES[491] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_491);
  VALUES[492] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_492);
  VALUES[493] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_493);
  VALUES[494] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_494);
  VALUES[495] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_495);
  VALUES[496] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_496);
  VALUES[497] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_497);
  VALUES[498] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_498);
  VALUES[499] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_499);
  VALUES[500] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_500);
  VALUES[501] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_501);
  VALUES[502] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_502);
  VALUES[503] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_503);
  VALUES[504] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_504);
  VALUES[505] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_505);
  VALUES[506] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_506);
  VALUES[507] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_507);
  VALUES[508] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_508);
  VALUES[509] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_509);
  VALUES[510] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_510);
  VALUES[511] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_511);
  VALUES[512] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_512);
  VALUES[513] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_513);
  VALUES[514] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_514);
  VALUES[515] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_515);
  VALUES[516] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_516);
  VALUES[517] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_517);
  VALUES[518] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_518);
  VALUES[519] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_519);
  VALUES[520] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_520);
  VALUES[521] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_521);
  VALUES[522] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_522);
  VALUES[523] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_523);
  VALUES[524] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_524);
  VALUES[525] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_525);
  VALUES[526] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_526);
  VALUES[527] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_527);
  VALUES[528] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_528);
  VALUES[529] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_529);
  VALUES[530] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_530);
  VALUES[531] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_531);
  VALUES[532] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_532);
  VALUES[533] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_533);
  VALUES[534] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_534);
  VALUES[535] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_535);
  VALUES[536] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_536);
  VALUES[537] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_537);
  VALUES[538] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_538);
  VALUES[539] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_539);
  VALUES[540] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_540);
  VALUES[541] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_541);
  VALUES[542] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_542);
  VALUES[543] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_543);
  VALUES[544] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_544);
  VALUES[545] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_545);
  VALUES[546] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_546);
  VALUES[547] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_547);
  VALUES[548] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_548);
  VALUES[549] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_549);
  VALUES[550] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_550);
  VALUES[551] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_551);
  VALUES[552] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_552);
  VALUES[553] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_553);
  VALUES[554] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_554);
  VALUES[555] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_555);
  VALUES[556] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_556);
  VALUES[557] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_557);
  VALUES[558] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_558);
  VALUES[559] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_559);
  VALUES[560] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_560);
  VALUES[561] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_561);
  VALUES[562] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_562);
  VALUES[563] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_563);
  VALUES[564] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_564);
  VALUES[565] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_565);
  VALUES[566] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_566);
  VALUES[567] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_567);
  VALUES[568] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_568);
  VALUES[569] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_569);
  VALUES[570] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_570);
  VALUES[571] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_571);
  VALUES[572] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_572);
  VALUES[573] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_573);
  VALUES[574] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_574);
  VALUES[575] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_575);
  VALUES[576] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_576);
  VALUES[577] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_577);
  VALUES[578] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_578);
  VALUES[579] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_579);
  VALUES[580] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_580);
  VALUES[581] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_581);
  VALUES[582] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_582);
  VALUES[583] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_583);
  VALUES[584] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_584);
  VALUES[585] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_585);
  VALUES[586] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_586);
  VALUES[587] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_587);
  VALUES[588] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_588);
  VALUES[589] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_589);
  VALUES[590] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_590);
  VALUES[591] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_591);
  VALUES[592] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_592);
  VALUES[593] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_593);
  VALUES[594] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_594);
  VALUES[595] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_595);
  VALUES[596] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_596);
  VALUES[597] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_597);
  VALUES[598] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_598);
  VALUES[599] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_599);
  VALUES[600] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_600);
  VALUES[601] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_601);
  VALUES[602] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_602);
  VALUES[603] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_603);
  VALUES[604] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_604);
  VALUES[605] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_605);
  VALUES[606] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_606);
  VALUES[607] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_607);
  VALUES[608] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_608);
  VALUES[609] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_609);
  VALUES[610] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_610);
  VALUES[611] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_611);
  VALUES[612] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_612);
  VALUES[613] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_613);
  VALUES[614] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_614);
  VALUES[615] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_615);
  VALUES[616] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_616);
  VALUES[617] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_617);
  VALUES[618] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_618);
  VALUES[619] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_619);
  VALUES[620] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_620);
  VALUES[621] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_621);
  VALUES[622] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_622);
  VALUES[623] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_623);
  VALUES[624] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_624);
  VALUES[625] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_625);
  VALUES[626] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_626);
  VALUES[627] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_627);
  VALUES[628] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_628);
  VALUES[629] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_629);
  VALUES[630] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_630);
  VALUES[631] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_631);
  VALUES[632] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_632);
  VALUES[633] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_633);
  VALUES[634] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_634);
  VALUES[635] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_635);
  VALUES[636] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_636);
  VALUES[637] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_637);
  VALUES[638] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_638);
  VALUES[639] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_639);
  VALUES[640] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_640);
  VALUES[641] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_641);
  VALUES[642] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_642);
  VALUES[643] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_643);
  VALUES[644] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_644);
  VALUES[645] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_645);
  VALUES[646] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_646);
  VALUES[647] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_647);
  VALUES[648] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_648);
  VALUES[649] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_649);
  VALUES[650] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_650);
  VALUES[651] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_651);
  VALUES[652] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_652);
  VALUES[653] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_653);
  VALUES[654] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_654);
  VALUES[655] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_655);
  VALUES[656] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_656);
  VALUES[657] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_657);
  VALUES[658] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_658);
  VALUES[659] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_659);
  VALUES[660] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_660);
  VALUES[661] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_661);
  VALUES[662] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_662);
  VALUES[663] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_663);
  VALUES[664] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_664);
  VALUES[665] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_665);
  VALUES[666] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_666);
  VALUES[667] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_667);
  VALUES[668] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_668);
  VALUES[669] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_669);
  VALUES[670] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_670);
  VALUES[671] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_671);
  VALUES[672] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_672);
  VALUES[673] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_673);
  VALUES[674] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_674);
  VALUES[675] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_675);
  VALUES[676] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_676);
  VALUES[677] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_677);
  VALUES[678] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_678);
  VALUES[679] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_679);
  VALUES[680] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_680);
  VALUES[681] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_681);
  VALUES[682] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_682);
  VALUES[683] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_683);
  VALUES[684] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_684);
  VALUES[685] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_685);
  VALUES[686] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_686);
  VALUES[687] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_687);
  VALUES[688] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_688);
  VALUES[689] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_689);
  VALUES[690] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_690);
  VALUES[691] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_691);
  VALUES[692] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_692);
  VALUES[693] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_693);
  VALUES[694] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_694);
  VALUES[695] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_695);
  VALUES[696] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_696);
  VALUES[697] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_697);
  VALUES[698] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_698);
  VALUES[699] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_699);
  VALUES[700] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_700);
  VALUES[701] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_701);
  VALUES[702] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_702);
  VALUES[703] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_703);
  VALUES[704] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_704);
  VALUES[705] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_705);
  VALUES[706] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_706);
  VALUES[707] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_707);
  VALUES[708] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_708);
  VALUES[709] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_709);
  VALUES[710] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_710);
  VALUES[711] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_711);
  VALUES[712] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_712);
  VALUES[713] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_713);
  VALUES[714] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_714);
  VALUES[715] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_715);
  VALUES[716] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_716);
  VALUES[717] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_717);
  VALUES[718] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_718);
  VALUES[719] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_719);
  VALUES[720] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_720);
  VALUES[721] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_721);
  VALUES[722] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_722);
  VALUES[723] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_723);
  VALUES[724] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_724);
  VALUES[725] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_725);
  VALUES[726] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_726);
  VALUES[727] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_727);
  VALUES[728] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_728);
  VALUES[729] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_729);
  VALUES[730] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_730);
  VALUES[731] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_731);
  VALUES[732] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_732);
  VALUES[733] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_733);
  VALUES[734] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_734);
  VALUES[735] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_735);
  VALUES[736] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_736);
  VALUES[737] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_737);
  VALUES[738] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_738);
  VALUES[739] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_739);
  VALUES[740] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_740);
  VALUES[741] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_741);
  VALUES[742] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_742);
  VALUES[743] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_743);
  VALUES[744] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_744);
  VALUES[745] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_745);
  VALUES[746] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_746);
  VALUES[747] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_747);
  VALUES[748] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_748);
  VALUES[749] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_749);
  VALUES[750] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_750);
  VALUES[751] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_751);
  VALUES[752] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_752);
  VALUES[753] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_753);
  VALUES[754] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_754);
  VALUES[755] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_755);
  VALUES[756] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_756);
  VALUES[757] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_757);
  VALUES[758] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_758);
  VALUES[759] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_759);
  VALUES[760] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_760);
  VALUES[761] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_761);
  VALUES[762] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_762);
  VALUES[763] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_763);
  VALUES[764] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_764);
  VALUES[765] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_765);
  VALUES[766] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_766);
  VALUES[767] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_767);
  VALUES[768] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_768);
  VALUES[769] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_769);
  VALUES[770] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_770);
  VALUES[771] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_771);
  VALUES[772] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_772);
  VALUES[773] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_773);
  VALUES[774] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_774);
  VALUES[775] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_775);
  VALUES[776] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_776);
  VALUES[777] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_777);
  VALUES[778] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_778);
  VALUES[779] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_779);
  VALUES[780] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_780);
  VALUES[781] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_781);
  VALUES[782] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_782);
  VALUES[783] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_783);
  VALUES[784] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_784);
  VALUES[785] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_785);
  VALUES[786] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_786);
  VALUES[787] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_787);
  VALUES[788] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_788);
  VALUES[789] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_789);
  VALUES[790] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_790);
  VALUES[791] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_791);
  VALUES[792] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_792);
  VALUES[793] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_793);
  VALUES[794] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_794);
  VALUES[795] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_795);
  VALUES[796] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_796);
  VALUES[797] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_797);
  VALUES[798] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_798);
  VALUES[799] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_799);
  VALUES[800] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_800);
  VALUES[801] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_801);
  VALUES[802] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_802);
  VALUES[803] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_803);
  VALUES[804] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_804);
  VALUES[805] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_805);
  VALUES[806] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_806);
  VALUES[807] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_807);
  VALUES[808] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_808);
  VALUES[809] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_809);
  VALUES[810] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_810);
  VALUES[811] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_811);
  VALUES[812] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_812);
  VALUES[813] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_813);
  VALUES[814] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_814);
  VALUES[815] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_815);
  VALUES[816] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_816);
  VALUES[817] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_817);
  VALUES[818] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_818);
  VALUES[819] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_819);
  VALUES[820] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_820);
  VALUES[821] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_821);
  VALUES[822] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_822);
  VALUES[823] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_823);
  VALUES[824] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_824);
  VALUES[825] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_825);
  VALUES[826] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_826);
  VALUES[827] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_827);
  VALUES[828] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_828);
  VALUES[829] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_829);
  VALUES[830] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_830);
  VALUES[831] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_831);
  VALUES[832] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_832);
  VALUES[833] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_833);
  VALUES[834] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_834);
  VALUES[835] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_835);
  VALUES[836] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_836);
  VALUES[837] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_837);
  VALUES[838] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_838);
  VALUES[839] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_839);
  VALUES[840] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_840);
  VALUES[841] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_841);
  VALUES[842] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_842);
  VALUES[843] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_843);
  VALUES[844] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_844);
  VALUES[845] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_845);
  VALUES[846] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_846);
  VALUES[847] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_847);
  VALUES[848] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_848);
  VALUES[849] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_849);
  VALUES[850] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_850);
  VALUES[851] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_851);
  VALUES[852] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_852);
  VALUES[853] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_853);
  VALUES[854] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_854);
  VALUES[855] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_855);
  VALUES[856] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_856);
  VALUES[857] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_857);
  VALUES[858] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_858);
  VALUES[859] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_859);
  VALUES[860] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_860);
  VALUES[861] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_861);
  VALUES[862] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_862);
  VALUES[863] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_863);
  VALUES[864] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_864);
  VALUES[865] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_865);
  VALUES[866] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_866);
  VALUES[867] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_867);
  VALUES[868] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_868);
  VALUES[869] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_869);
  VALUES[870] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_870);
  VALUES[871] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_871);
  VALUES[872] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_872);
  VALUES[873] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_873);
  VALUES[874] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_874);
  VALUES[875] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_875);
  VALUES[876] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_876);
  VALUES[877] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_877);
  VALUES[878] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_878);
  VALUES[879] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_879);
  VALUES[880] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_880);
  VALUES[881] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_881);
  VALUES[882] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_882);
  VALUES[883] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_883);
  VALUES[884] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_884);
  VALUES[885] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_885);
  VALUES[886] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_886);
  VALUES[887] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_887);
  VALUES[888] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_888);
  VALUES[889] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_889);
  VALUES[890] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_890);
  VALUES[891] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_891);
  VALUES[892] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_892);
  VALUES[893] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_893);
  VALUES[894] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_894);
  VALUES[895] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_895);
  VALUES[896] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_896);
  VALUES[897] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_897);
  VALUES[898] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_898);
  VALUES[899] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_899);
  VALUES[900] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_900);
  VALUES[901] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_901);
  VALUES[902] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_902);
  VALUES[903] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_903);
  VALUES[904] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_904);
  VALUES[905] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_905);
  VALUES[906] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_906);
  VALUES[907] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_907);
  VALUES[908] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_908);
  VALUES[909] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_909);
  VALUES[910] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_910);
  VALUES[911] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_911);
  VALUES[912] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_912);
  VALUES[913] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_913);
  VALUES[914] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_914);
  VALUES[915] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_915);
  VALUES[916] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_916);
  VALUES[917] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_917);
  VALUES[918] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_918);
  VALUES[919] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_919);
  VALUES[920] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_920);
  VALUES[921] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_921);
  VALUES[922] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_922);
  VALUES[923] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_923);
  VALUES[924] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_924);
  VALUES[925] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_925);
  VALUES[926] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_926);
  VALUES[927] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_927);
  VALUES[928] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_928);
  VALUES[929] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_929);
  VALUES[930] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_930);
  VALUES[931] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_931);
  VALUES[932] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_932);
  VALUES[933] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_933);
  VALUES[934] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_934);
  VALUES[935] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_935);
  VALUES[936] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_936);
  VALUES[937] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_937);
  VALUES[938] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_938);
  VALUES[939] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_939);
  VALUES[940] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_940);
  VALUES[941] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_941);
  VALUES[942] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_942);
  VALUES[943] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_943);
  VALUES[944] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_944);
  VALUES[945] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_945);
  VALUES[946] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_946);
  VALUES[947] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_947);
  VALUES[948] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_948);
  VALUES[949] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_949);
  VALUES[950] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_950);
  VALUES[951] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_951);
  VALUES[952] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_952);
  VALUES[953] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_953);
  VALUES[954] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_954);
  VALUES[955] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_955);
  VALUES[956] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_956);
  VALUES[957] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_957);
  VALUES[958] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_958);
  VALUES[959] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_959);
  VALUES[960] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_960);
  VALUES[961] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_961);
  VALUES[962] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_962);
  VALUES[963] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_963);
  VALUES[964] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_964);
  VALUES[965] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_965);
  VALUES[966] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_966);
  VALUES[967] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_967);
  VALUES[968] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_968);
  VALUES[969] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_969);
  VALUES[970] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_970);
  VALUES[971] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_971);
  VALUES[972] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_972);
  VALUES[973] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_973);
  VALUES[974] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_974);
  VALUES[975] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_975);
  VALUES[976] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_976);
  VALUES[977] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_977);
  VALUES[978] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_978);
  VALUES[979] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_979);
  VALUES[980] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_980);
  VALUES[981] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_981);
  VALUES[982] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_982);
  VALUES[983] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_983);
  VALUES[984] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_984);
  VALUES[985] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_985);
  VALUES[986] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_986);
  VALUES[987] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_987);
  VALUES[988] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_988);
  VALUES[989] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_989);
  VALUES[990] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_990);
  VALUES[991] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_991);
  VALUES[992] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_992);
  VALUES[993] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_993);
  VALUES[994] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_994);
  VALUES[995] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_995);
  VALUES[996] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_996);
  VALUES[997] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_997);
  VALUES[998] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_998);
  VALUES[999] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_999);
  VALUES[1000] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1000);
  VALUES[1001] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1001);
  VALUES[1002] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1002);
  VALUES[1003] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1003);
  VALUES[1004] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1004);
  VALUES[1005] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1005);
  VALUES[1006] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1006);
  VALUES[1007] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1007);
  VALUES[1008] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1008);
  VALUES[1009] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1009);
  VALUES[1010] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1010);
  VALUES[1011] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1011);
  VALUES[1012] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1012);
  VALUES[1013] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1013);
  VALUES[1014] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1014);
  VALUES[1015] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1015);
  VALUES[1016] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1016);
  VALUES[1017] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1017);
  VALUES[1018] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1018);
  VALUES[1019] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1019);
  VALUES[1020] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1020);
  VALUES[1021] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1021);
  VALUES[1022] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1022);
  VALUES[1023] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1023);
  VALUES[1024] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1024);
  VALUES[1025] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1025);
  VALUES[1026] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1026);
  VALUES[1027] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1027);
  VALUES[1028] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1028);
  VALUES[1029] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1029);
  VALUES[1030] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1030);
  VALUES[1031] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1031);
  VALUES[1032] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1032);
  VALUES[1033] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1033);
  VALUES[1034] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1034);
  VALUES[1035] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1035);
  VALUES[1036] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1036);
  VALUES[1037] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1037);
  VALUES[1038] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1038);
  VALUES[1039] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1039);
  VALUES[1040] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1040);
  VALUES[1041] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1041);
  VALUES[1042] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1042);
  VALUES[1043] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1043);
  VALUES[1044] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1044);
  VALUES[1045] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1045);
  VALUES[1046] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1046);
  VALUES[1047] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1047);
  VALUES[1048] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1048);
  VALUES[1049] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1049);
  VALUES[1050] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1050);
  VALUES[1051] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1051);
  VALUES[1052] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1052);
  VALUES[1053] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1053);
  VALUES[1054] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1054);
  VALUES[1055] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1055);
  VALUES[1056] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1056);
  VALUES[1057] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1057);
  VALUES[1058] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1058);
  VALUES[1059] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1059);
  VALUES[1060] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1060);
  VALUES[1061] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1061);
  VALUES[1062] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1062);
  VALUES[1063] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1063);
  VALUES[1064] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1064);
  VALUES[1065] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1065);
  VALUES[1066] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1066);
  VALUES[1067] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1067);
  VALUES[1068] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1068);
  VALUES[1069] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1069);
  VALUES[1070] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1070);
  VALUES[1071] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1071);
  VALUES[1072] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1072);
  VALUES[1073] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1073);
  VALUES[1074] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1074);
  VALUES[1075] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1075);
  VALUES[1076] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1076);
  VALUES[1077] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1077);
  VALUES[1078] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1078);
  VALUES[1079] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1079);
  VALUES[1080] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1080);
  VALUES[1081] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1081);
  VALUES[1082] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1082);
  VALUES[1083] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1083);
  VALUES[1084] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1084);
  VALUES[1085] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1085);
  VALUES[1086] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1086);
  VALUES[1087] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1087);
  VALUES[1088] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1088);
  VALUES[1089] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1089);
  VALUES[1090] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1090);
  VALUES[1091] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1091);
  VALUES[1092] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1092);
  VALUES[1093] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1093);
  VALUES[1094] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1094);
  VALUES[1095] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1095);
  VALUES[1096] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1096);
  VALUES[1097] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1097);
  VALUES[1098] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1098);
  VALUES[1099] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1099);
  VALUES[1100] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1100);
  VALUES[1101] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1101);
  VALUES[1102] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1102);
  VALUES[1103] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1103);
  VALUES[1104] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1104);
  VALUES[1105] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1105);
  VALUES[1106] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1106);
  VALUES[1107] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1107);
  VALUES[1108] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1108);
  VALUES[1109] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1109);
  VALUES[1110] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1110);
  VALUES[1111] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1111);
  VALUES[1112] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1112);
  VALUES[1113] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1113);
  VALUES[1114] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1114);
  VALUES[1115] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1115);
  VALUES[1116] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1116);
  VALUES[1117] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1117);
  VALUES[1118] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1118);
  VALUES[1119] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1119);
  VALUES[1120] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1120);
  VALUES[1121] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1121);
  VALUES[1122] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1122);
  VALUES[1123] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1123);
  VALUES[1124] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1124);
  VALUES[1125] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1125);
  VALUES[1126] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1126);
  VALUES[1127] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1127);
  VALUES[1128] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1128);
  VALUES[1129] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1129);
  VALUES[1130] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1130);
  VALUES[1131] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1131);
  VALUES[1132] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1132);
  VALUES[1133] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1133);
  VALUES[1134] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1134);
  VALUES[1135] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1135);
  VALUES[1136] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1136);
  VALUES[1137] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1137);
  VALUES[1138] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1138);
  VALUES[1139] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1139);
  VALUES[1140] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1140);
  VALUES[1141] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1141);
  VALUES[1142] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1142);
  VALUES[1143] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1143);
  VALUES[1144] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1144);
  VALUES[1145] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1145);
  VALUES[1146] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1146);
  VALUES[1147] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1147);
  VALUES[1148] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1148);
  VALUES[1149] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1149);
  VALUES[1150] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1150);
  VALUES[1151] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1151);
  VALUES[1152] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1152);
  VALUES[1153] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1153);
  VALUES[1154] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1154);
  VALUES[1155] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1155);
  VALUES[1156] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1156);
  VALUES[1157] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1157);
  VALUES[1158] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1158);
  VALUES[1159] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1159);
  VALUES[1160] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1160);
  VALUES[1161] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1161);
  VALUES[1162] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1162);
  VALUES[1163] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1163);
  VALUES[1164] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1164);
  VALUES[1165] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1165);
  VALUES[1166] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1166);
  VALUES[1167] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1167);
  VALUES[1168] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1168);
  VALUES[1169] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1169);
  VALUES[1170] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1170);
  VALUES[1171] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1171);
  VALUES[1172] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1172);
  VALUES[1173] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1173);
  VALUES[1174] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1174);
  VALUES[1175] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1175);
  VALUES[1176] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1176);
  VALUES[1177] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1177);
  VALUES[1178] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1178);
  VALUES[1179] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1179);
  VALUES[1180] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1180);
  VALUES[1181] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1181);
  VALUES[1182] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1182);
  VALUES[1183] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1183);
  VALUES[1184] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1184);
  VALUES[1185] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1185);
  VALUES[1186] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1186);
  VALUES[1187] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1187);
  VALUES[1188] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1188);
  VALUES[1189] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1189);
  VALUES[1190] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1190);
  VALUES[1191] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1191);
  VALUES[1192] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1192);
  VALUES[1193] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1193);
  VALUES[1194] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1194);
  VALUES[1195] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1195);
  VALUES[1196] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1196);
  VALUES[1197] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1197);
  VALUES[1198] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1198);
  VALUES[1199] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1199);
  VALUES[1200] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1200);
  VALUES[1201] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1201);
  VALUES[1202] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1202);
  VALUES[1203] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1203);
  VALUES[1204] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1204);
  VALUES[1205] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1205);
  VALUES[1206] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1206);
  VALUES[1207] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1207);
  VALUES[1208] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1208);
  VALUES[1209] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1209);
  VALUES[1210] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1210);
  VALUES[1211] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1211);
  VALUES[1212] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1212);
  VALUES[1213] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1213);
  VALUES[1214] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1214);
  VALUES[1215] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1215);
  VALUES[1216] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1216);
  VALUES[1217] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1217);
  VALUES[1218] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1218);
  VALUES[1219] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1219);
  VALUES[1220] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1220);
  VALUES[1221] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1221);
  VALUES[1222] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1222);
  VALUES[1223] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1223);
  VALUES[1224] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1224);
  VALUES[1225] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1225);
  VALUES[1226] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1226);
  VALUES[1227] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1227);
  VALUES[1228] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1228);
  VALUES[1229] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1229);
  VALUES[1230] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1230);
  VALUES[1231] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1231);
  VALUES[1232] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1232);
  VALUES[1233] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1233);
  VALUES[1234] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1234);
  VALUES[1235] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1235);
  VALUES[1236] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1236);
  VALUES[1237] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1237);
  VALUES[1238] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1238);
  VALUES[1239] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1239);
  VALUES[1240] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1240);
  VALUES[1241] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1241);
  VALUES[1242] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1242);
  VALUES[1243] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1243);
  VALUES[1244] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1244);
  VALUES[1245] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1245);
  VALUES[1246] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1246);
  VALUES[1247] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1247);
  VALUES[1248] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1248);
  VALUES[1249] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1249);
  VALUES[1250] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1250);
  VALUES[1251] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1251);
  VALUES[1252] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1252);
  VALUES[1253] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1253);
  VALUES[1254] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1254);
  VALUES[1255] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1255);
  VALUES[1256] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1256);
  VALUES[1257] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1257);
  VALUES[1258] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1258);
  VALUES[1259] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1259);
  VALUES[1260] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1260);
  VALUES[1261] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1261);
  VALUES[1262] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1262);
  VALUES[1263] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1263);
  VALUES[1264] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1264);
  VALUES[1265] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1265);
  VALUES[1266] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1266);
  VALUES[1267] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1267);
  VALUES[1268] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1268);
  VALUES[1269] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1269);
  VALUES[1270] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1270);
  VALUES[1271] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1271);
  VALUES[1272] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1272);
  VALUES[1273] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1273);
  VALUES[1274] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1274);
  VALUES[1275] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1275);
  VALUES[1276] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1276);
  VALUES[1277] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1277);
  VALUES[1278] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1278);
  VALUES[1279] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1279);
  VALUES[1280] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1280);
  VALUES[1281] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1281);
  VALUES[1282] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1282);
  VALUES[1283] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1283);
  VALUES[1284] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1284);
  VALUES[1285] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1285);
  VALUES[1286] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1286);
  VALUES[1287] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1287);
  VALUES[1288] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1288);
  VALUES[1289] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1289);
  VALUES[1290] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1290);
  VALUES[1291] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1291);
  VALUES[1292] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1292);
  VALUES[1293] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1293);
  VALUES[1294] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1294);
  VALUES[1295] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1295);
  VALUES[1296] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1296);
  VALUES[1297] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1297);
  VALUES[1298] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1298);
  VALUES[1299] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1299);
  VALUES[1300] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1300);
  VALUES[1301] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1301);
  VALUES[1302] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1302);
  VALUES[1303] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1303);
  VALUES[1304] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1304);
  VALUES[1305] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1305);
  VALUES[1306] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1306);
  VALUES[1307] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1307);
  VALUES[1308] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1308);
  VALUES[1309] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1309);
  VALUES[1310] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1310);
  VALUES[1311] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1311);
  VALUES[1312] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1312);
  VALUES[1313] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1313);
  VALUES[1314] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1314);
  VALUES[1315] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1315);
  VALUES[1316] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1316);
  VALUES[1317] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1317);
  VALUES[1318] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1318);
  VALUES[1319] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1319);
  VALUES[1320] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1320);
  VALUES[1321] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1321);
  VALUES[1322] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1322);
  VALUES[1323] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1323);
  VALUES[1324] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1324);
  VALUES[1325] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1325);
  VALUES[1326] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1326);
  VALUES[1327] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1327);
  VALUES[1328] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1328);
  VALUES[1329] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1329);
  VALUES[1330] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1330);
  VALUES[1331] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1331);
  VALUES[1332] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1332);
  VALUES[1333] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1333);
  VALUES[1334] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1334);
  VALUES[1335] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1335);
  VALUES[1336] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1336);
  VALUES[1337] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1337);
  VALUES[1338] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1338);
  VALUES[1339] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1339);
  VALUES[1340] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1340);
  VALUES[1341] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1341);
  VALUES[1342] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1342);
  VALUES[1343] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1343);
  VALUES[1344] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1344);
  VALUES[1345] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1345);
  VALUES[1346] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1346);
  VALUES[1347] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1347);
  VALUES[1348] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1348);
  VALUES[1349] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1349);
  VALUES[1350] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1350);
  VALUES[1351] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1351);
  VALUES[1352] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1352);
  VALUES[1353] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1353);
  VALUES[1354] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1354);
  VALUES[1355] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1355);
  VALUES[1356] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1356);
  VALUES[1357] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1357);
  VALUES[1358] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1358);
  VALUES[1359] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1359);
  VALUES[1360] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1360);
  VALUES[1361] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1361);
  VALUES[1362] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1362);
  VALUES[1363] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1363);
  VALUES[1364] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1364);
  VALUES[1365] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1365);
  VALUES[1366] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1366);
  VALUES[1367] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1367);
  VALUES[1368] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1368);
  VALUES[1369] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1369);
  VALUES[1370] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1370);
  VALUES[1371] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1371);
  VALUES[1372] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1372);
  VALUES[1373] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1373);
  VALUES[1374] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1374);
  VALUES[1375] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1375);
  VALUES[1376] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1376);
  VALUES[1377] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1377);
  VALUES[1378] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1378);
  VALUES[1379] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1379);
  VALUES[1380] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1380);
  VALUES[1381] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1381);
  VALUES[1382] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1382);
  VALUES[1383] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1383);
  VALUES[1384] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1384);
  VALUES[1385] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1385);
  VALUES[1386] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1386);
  VALUES[1387] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1387);
  VALUES[1388] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1388);
  VALUES[1389] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1389);
  VALUES[1390] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1390);
  VALUES[1391] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1391);
  VALUES[1392] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1392);
  VALUES[1393] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1393);
  VALUES[1394] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1394);
  VALUES[1395] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1395);
  VALUES[1396] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1396);
  VALUES[1397] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1397);
  VALUES[1398] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1398);
  VALUES[1399] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1399);
  VALUES[1400] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1400);
  VALUES[1401] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1401);
  VALUES[1402] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1402);
  VALUES[1403] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1403);
  VALUES[1404] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1404);
  VALUES[1405] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1405);
  VALUES[1406] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1406);
  VALUES[1407] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1407);
  VALUES[1408] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1408);
  VALUES[1409] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1409);
  VALUES[1410] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1410);
  VALUES[1411] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1411);
  VALUES[1412] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1412);
  VALUES[1413] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1413);
  VALUES[1414] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1414);
  VALUES[1415] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1415);
  VALUES[1416] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1416);
  VALUES[1417] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1417);
  VALUES[1418] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1418);
  VALUES[1419] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1419);
  VALUES[1420] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1420);
  VALUES[1421] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1421);
  VALUES[1422] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1422);
  VALUES[1423] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1423);
  VALUES[1424] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1424);
  VALUES[1425] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1425);
  VALUES[1426] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1426);
  VALUES[1427] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1427);
  VALUES[1428] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1428);
  VALUES[1429] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1429);
  VALUES[1430] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1430);
  VALUES[1431] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1431);
  VALUES[1432] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1432);
  VALUES[1433] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1433);
  VALUES[1434] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1434);
  VALUES[1435] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1435);
  VALUES[1436] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1436);
  VALUES[1437] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1437);
  VALUES[1438] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1438);
  VALUES[1439] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1439);
  VALUES[1440] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1440);
  VALUES[1441] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1441);
  VALUES[1442] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1442);
  VALUES[1443] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1443);
  VALUES[1444] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1444);
  VALUES[1445] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1445);
  VALUES[1446] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1446);
  VALUES[1447] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1447);
  VALUES[1448] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1448);
  VALUES[1449] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1449);
  VALUES[1450] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1450);
  VALUES[1451] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1451);
  VALUES[1452] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1452);
  VALUES[1453] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1453);
  VALUES[1454] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1454);
  VALUES[1455] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1455);
  VALUES[1456] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1456);
  VALUES[1457] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1457);
  VALUES[1458] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1458);
  VALUES[1459] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1459);
  VALUES[1460] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1460);
  VALUES[1461] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1461);
  VALUES[1462] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1462);
  VALUES[1463] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1463);
  VALUES[1464] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1464);
  VALUES[1465] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1465);
  VALUES[1466] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1466);
  VALUES[1467] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1467);
  VALUES[1468] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1468);
  VALUES[1469] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1469);
  VALUES[1470] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1470);
  VALUES[1471] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1471);
  VALUES[1472] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1472);
  VALUES[1473] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1473);
  VALUES[1474] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1474);
  VALUES[1475] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1475);
  VALUES[1476] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1476);
  VALUES[1477] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1477);
  VALUES[1478] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1478);
  VALUES[1479] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1479);
  VALUES[1480] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1480);
  VALUES[1481] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1481);
  VALUES[1482] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1482);
  VALUES[1483] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1483);
  VALUES[1484] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1484);
  VALUES[1485] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1485);
  VALUES[1486] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1486);
  VALUES[1487] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1487);
  VALUES[1488] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1488);
  VALUES[1489] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1489);
  VALUES[1490] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1490);
  VALUES[1491] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1491);
  VALUES[1492] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1492);
  VALUES[1493] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1493);
  VALUES[1494] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1494);
  VALUES[1495] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1495);
  VALUES[1496] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1496);
  VALUES[1497] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1497);
  VALUES[1498] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1498);
  VALUES[1499] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1499);
  VALUES[1500] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1500);
  VALUES[1501] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1501);
  VALUES[1502] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1502);
  VALUES[1503] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1503);
  VALUES[1504] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1504);
  VALUES[1505] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1505);
  VALUES[1506] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1506);
  VALUES[1507] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1507);
  VALUES[1508] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1508);
  VALUES[1509] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1509);
  VALUES[1510] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1510);
  VALUES[1511] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1511);
  VALUES[1512] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1512);
  VALUES[1513] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1513);
  VALUES[1514] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1514);
  VALUES[1515] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1515);
  VALUES[1516] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1516);
  VALUES[1517] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1517);
  VALUES[1518] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1518);
  VALUES[1519] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1519);
  VALUES[1520] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1520);
  VALUES[1521] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1521);
  VALUES[1522] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1522);
  VALUES[1523] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1523);
  VALUES[1524] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1524);
  VALUES[1525] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1525);
  VALUES[1526] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1526);
  VALUES[1527] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1527);
  VALUES[1528] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1528);
  VALUES[1529] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1529);
  VALUES[1530] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1530);
  VALUES[1531] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1531);
  VALUES[1532] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1532);
  VALUES[1533] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1533);
  VALUES[1534] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1534);
  VALUES[1535] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1535);
  VALUES[1536] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1536);
  VALUES[1537] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1537);
  VALUES[1538] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1538);
  VALUES[1539] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1539);
  VALUES[1540] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1540);
  VALUES[1541] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1541);
  VALUES[1542] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1542);
  VALUES[1543] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1543);
  VALUES[1544] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1544);
  VALUES[1545] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1545);
  VALUES[1546] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1546);
  VALUES[1547] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1547);
  VALUES[1548] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1548);
  VALUES[1549] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1549);
  VALUES[1550] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1550);
  VALUES[1551] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1551);
  VALUES[1552] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1552);
  VALUES[1553] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1553);
  VALUES[1554] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1554);
  VALUES[1555] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1555);
  VALUES[1556] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1556);
  VALUES[1557] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1557);
  VALUES[1558] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1558);
  VALUES[1559] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1559);
  VALUES[1560] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1560);
  VALUES[1561] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1561);
  VALUES[1562] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1562);
  VALUES[1563] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1563);
  VALUES[1564] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1564);
  VALUES[1565] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1565);
  VALUES[1566] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1566);
  VALUES[1567] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1567);
  VALUES[1568] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1568);
  VALUES[1569] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1569);
  VALUES[1570] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1570);
  VALUES[1571] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1571);
  VALUES[1572] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1572);
  VALUES[1573] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1573);
  VALUES[1574] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1574);
  VALUES[1575] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1575);
  VALUES[1576] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1576);
  VALUES[1577] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1577);
  VALUES[1578] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1578);
  VALUES[1579] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1579);
  VALUES[1580] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1580);
  VALUES[1581] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1581);
  VALUES[1582] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1582);
  VALUES[1583] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1583);
  VALUES[1584] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1584);
  VALUES[1585] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1585);
  VALUES[1586] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1586);
  VALUES[1587] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1587);
  VALUES[1588] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1588);
  VALUES[1589] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1589);
  VALUES[1590] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1590);
  VALUES[1591] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1591);
  VALUES[1592] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1592);
  VALUES[1593] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1593);
  VALUES[1594] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1594);
  VALUES[1595] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1595);
  VALUES[1596] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1596);
  VALUES[1597] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1597);
  VALUES[1598] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1598);
  VALUES[1599] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1599);
  VALUES[1600] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1600);
  VALUES[1601] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1601);
  VALUES[1602] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1602);
  VALUES[1603] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1603);
  VALUES[1604] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1604);
  VALUES[1605] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1605);
  VALUES[1606] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1606);
  VALUES[1607] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1607);
  VALUES[1608] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1608);
  VALUES[1609] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1609);
  VALUES[1610] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1610);
  VALUES[1611] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1611);
  VALUES[1612] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1612);
  VALUES[1613] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1613);
  VALUES[1614] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1614);
  VALUES[1615] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1615);
  VALUES[1616] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1616);
  VALUES[1617] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1617);
  VALUES[1618] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1618);
  VALUES[1619] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1619);
  VALUES[1620] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1620);
  VALUES[1621] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1621);
  VALUES[1622] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1622);
  VALUES[1623] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1623);
  VALUES[1624] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1624);
  VALUES[1625] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1625);
  VALUES[1626] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1626);
  VALUES[1627] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1627);
  VALUES[1628] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1628);
  VALUES[1629] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1629);
  VALUES[1630] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1630);
  VALUES[1631] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1631);
  VALUES[1632] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1632);
  VALUES[1633] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1633);
  VALUES[1634] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1634);
  VALUES[1635] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1635);
  VALUES[1636] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1636);
  VALUES[1637] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1637);
  VALUES[1638] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1638);
  VALUES[1639] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1639);
  VALUES[1640] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1640);
  VALUES[1641] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1641);
  VALUES[1642] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1642);
  VALUES[1643] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1643);
  VALUES[1644] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1644);
  VALUES[1645] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1645);
  VALUES[1646] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1646);
  VALUES[1647] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1647);
  VALUES[1648] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1648);
  VALUES[1649] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1649);
  VALUES[1650] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1650);
  VALUES[1651] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1651);
  VALUES[1652] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1652);
  VALUES[1653] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1653);
  VALUES[1654] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1654);
  VALUES[1655] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1655);
  VALUES[1656] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1656);
  VALUES[1657] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1657);
  VALUES[1658] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1658);
  VALUES[1659] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1659);
  VALUES[1660] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1660);
  VALUES[1661] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1661);
  VALUES[1662] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1662);
  VALUES[1663] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1663);
  VALUES[1664] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1664);
  VALUES[1665] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1665);
  VALUES[1666] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1666);
  VALUES[1667] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1667);
  VALUES[1668] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1668);
  VALUES[1669] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1669);
  VALUES[1670] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1670);
  VALUES[1671] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1671);
  VALUES[1672] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1672);
  VALUES[1673] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1673);
  VALUES[1674] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1674);
  VALUES[1675] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1675);
  VALUES[1676] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1676);
  VALUES[1677] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1677);
  VALUES[1678] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1678);
  VALUES[1679] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1679);
  VALUES[1680] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1680);
  VALUES[1681] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1681);
  VALUES[1682] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1682);
  VALUES[1683] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1683);
  VALUES[1684] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1684);
  VALUES[1685] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1685);
  VALUES[1686] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1686);
  VALUES[1687] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1687);
  VALUES[1688] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1688);
  VALUES[1689] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1689);
  VALUES[1690] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1690);
  VALUES[1691] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1691);
  VALUES[1692] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1692);
  VALUES[1693] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1693);
  VALUES[1694] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1694);
  VALUES[1695] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1695);
  VALUES[1696] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1696);
  VALUES[1697] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1697);
  VALUES[1698] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1698);
  VALUES[1699] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1699);
  VALUES[1700] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1700);
  VALUES[1701] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1701);
  VALUES[1702] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1702);
  VALUES[1703] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1703);
  VALUES[1704] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1704);
  VALUES[1705] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1705);
  VALUES[1706] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1706);
  VALUES[1707] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1707);
  VALUES[1708] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1708);
  VALUES[1709] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1709);
  VALUES[1710] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1710);
  VALUES[1711] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1711);
  VALUES[1712] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1712);
  VALUES[1713] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1713);
  VALUES[1714] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1714);
  VALUES[1715] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1715);
  VALUES[1716] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1716);
  VALUES[1717] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1717);
  VALUES[1718] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1718);
  VALUES[1719] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1719);
  VALUES[1720] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1720);
  VALUES[1721] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1721);
  VALUES[1722] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1722);
  VALUES[1723] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1723);
  VALUES[1724] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1724);
  VALUES[1725] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1725);
  VALUES[1726] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1726);
  VALUES[1727] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1727);
  VALUES[1728] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1728);
  VALUES[1729] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1729);
  VALUES[1730] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1730);
  VALUES[1731] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1731);
  VALUES[1732] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1732);
  VALUES[1733] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1733);
  VALUES[1734] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1734);
  VALUES[1735] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1735);
  VALUES[1736] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1736);
  VALUES[1737] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1737);
  VALUES[1738] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1738);
  VALUES[1739] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1739);
  VALUES[1740] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1740);
  VALUES[1741] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1741);
  VALUES[1742] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1742);
  VALUES[1743] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1743);
  VALUES[1744] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1744);
  VALUES[1745] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1745);
  VALUES[1746] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1746);
  VALUES[1747] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1747);
  VALUES[1748] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1748);
  VALUES[1749] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1749);
  VALUES[1750] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1750);
  VALUES[1751] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1751);
  VALUES[1752] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1752);
  VALUES[1753] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1753);
  VALUES[1754] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1754);
  VALUES[1755] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1755);
  VALUES[1756] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1756);
  VALUES[1757] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1757);
  VALUES[1758] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1758);
  VALUES[1759] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1759);
  VALUES[1760] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1760);
  VALUES[1761] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1761);
  VALUES[1762] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1762);
  VALUES[1763] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1763);
  VALUES[1764] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1764);
  VALUES[1765] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1765);
  VALUES[1766] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1766);
  VALUES[1767] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1767);
  VALUES[1768] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1768);
  VALUES[1769] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1769);
  VALUES[1770] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1770);
  VALUES[1771] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1771);
  VALUES[1772] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1772);
  VALUES[1773] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1773);
  VALUES[1774] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1774);
  VALUES[1775] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1775);
  VALUES[1776] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1776);
  VALUES[1777] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1777);
  VALUES[1778] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1778);
  VALUES[1779] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1779);
  VALUES[1780] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1780);
  VALUES[1781] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1781);
  VALUES[1782] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1782);
  VALUES[1783] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1783);
  VALUES[1784] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1784);
  VALUES[1785] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1785);
  VALUES[1786] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1786);
  VALUES[1787] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1787);
  VALUES[1788] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1788);
  VALUES[1789] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1789);
  VALUES[1790] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1790);
  VALUES[1791] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1791);
  VALUES[1792] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1792);
  VALUES[1793] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1793);
  VALUES[1794] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1794);
  VALUES[1795] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1795);
  VALUES[1796] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1796);
  VALUES[1797] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1797);
  VALUES[1798] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1798);
  VALUES[1799] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1799);
  VALUES[1800] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1800);
  VALUES[1801] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1801);
  VALUES[1802] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1802);
  VALUES[1803] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1803);
  VALUES[1804] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1804);
  VALUES[1805] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1805);
  VALUES[1806] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1806);
  VALUES[1807] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1807);
  VALUES[1808] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1808);
  VALUES[1809] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1809);
  VALUES[1810] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1810);
  VALUES[1811] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1811);
  VALUES[1812] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1812);
  VALUES[1813] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1813);
  VALUES[1814] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1814);
  VALUES[1815] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1815);
  VALUES[1816] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1816);
  VALUES[1817] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1817);
  VALUES[1818] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1818);
  VALUES[1819] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1819);
  VALUES[1820] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1820);
  VALUES[1821] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1821);
  VALUES[1822] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1822);
  VALUES[1823] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1823);
  VALUES[1824] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1824);
  VALUES[1825] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1825);
  VALUES[1826] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1826);
  VALUES[1827] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1827);
  VALUES[1828] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1828);
  VALUES[1829] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1829);
  VALUES[1830] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1830);
  VALUES[1831] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1831);
  VALUES[1832] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1832);
  VALUES[1833] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1833);
  VALUES[1834] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1834);
  VALUES[1835] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1835);
  VALUES[1836] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1836);
  VALUES[1837] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1837);
  VALUES[1838] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1838);
  VALUES[1839] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1839);
  VALUES[1840] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1840);
  VALUES[1841] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1841);
  VALUES[1842] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1842);
  VALUES[1843] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1843);
  VALUES[1844] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1844);
  VALUES[1845] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1845);
  VALUES[1846] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1846);
  VALUES[1847] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1847);
  VALUES[1848] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1848);
  VALUES[1849] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1849);
  VALUES[1850] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1850);
  VALUES[1851] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1851);
  VALUES[1852] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1852);
  VALUES[1853] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1853);
  VALUES[1854] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1854);
  VALUES[1855] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1855);
  VALUES[1856] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1856);
  VALUES[1857] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1857);
  VALUES[1858] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1858);
  VALUES[1859] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1859);
  VALUES[1860] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1860);
  VALUES[1861] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1861);
  VALUES[1862] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1862);
  VALUES[1863] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1863);
  VALUES[1864] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1864);
  VALUES[1865] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1865);
  VALUES[1866] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1866);
  VALUES[1867] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1867);
  VALUES[1868] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1868);
  VALUES[1869] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1869);
  VALUES[1870] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1870);
  VALUES[1871] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1871);
  VALUES[1872] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1872);
  VALUES[1873] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1873);
  VALUES[1874] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1874);
  VALUES[1875] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1875);
  VALUES[1876] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1876);
  VALUES[1877] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1877);
  VALUES[1878] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1878);
  VALUES[1879] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1879);
  VALUES[1880] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1880);
  VALUES[1881] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1881);
  VALUES[1882] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1882);
  VALUES[1883] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1883);
  VALUES[1884] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1884);
  VALUES[1885] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1885);
  VALUES[1886] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1886);
  VALUES[1887] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1887);
  VALUES[1888] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1888);
  VALUES[1889] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1889);
  VALUES[1890] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1890);
  VALUES[1891] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1891);
  VALUES[1892] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1892);
  VALUES[1893] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1893);
  VALUES[1894] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1894);
  VALUES[1895] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1895);
  VALUES[1896] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1896);
  VALUES[1897] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1897);
  VALUES[1898] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1898);
  VALUES[1899] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1899);
  VALUES[1900] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1900);
  VALUES[1901] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1901);
  VALUES[1902] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1902);
  VALUES[1903] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1903);
  VALUES[1904] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1904);
  VALUES[1905] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1905);
  VALUES[1906] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1906);
  VALUES[1907] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1907);
  VALUES[1908] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1908);
  VALUES[1909] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1909);
  VALUES[1910] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1910);
  VALUES[1911] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1911);
  VALUES[1912] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1912);
  VALUES[1913] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1913);
  VALUES[1914] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1914);
  VALUES[1915] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1915);
  VALUES[1916] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1916);
  VALUES[1917] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1917);
  VALUES[1918] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1918);
  VALUES[1919] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1919);
  VALUES[1920] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1920);
  VALUES[1921] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1921);
  VALUES[1922] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1922);
  VALUES[1923] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1923);
  VALUES[1924] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1924);
  VALUES[1925] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1925);
  VALUES[1926] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1926);
  VALUES[1927] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1927);
  VALUES[1928] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1928);
  VALUES[1929] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1929);
  VALUES[1930] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1930);
  VALUES[1931] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1931);
  VALUES[1932] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1932);
  VALUES[1933] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1933);
  VALUES[1934] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1934);
  VALUES[1935] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1935);
  VALUES[1936] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1936);
  VALUES[1937] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1937);
  VALUES[1938] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1938);
  VALUES[1939] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1939);
  VALUES[1940] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1940);
  VALUES[1941] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1941);
  VALUES[1942] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1942);
  VALUES[1943] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1943);
  VALUES[1944] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1944);
  VALUES[1945] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1945);
  VALUES[1946] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1946);
  VALUES[1947] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1947);
  VALUES[1948] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1948);
  VALUES[1949] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1949);
  VALUES[1950] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1950);
  VALUES[1951] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1951);
  VALUES[1952] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1952);
  VALUES[1953] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1953);
  VALUES[1954] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1954);
  VALUES[1955] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1955);
  VALUES[1956] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1956);
  VALUES[1957] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1957);
  VALUES[1958] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1958);
  VALUES[1959] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1959);
  VALUES[1960] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1960);
  VALUES[1961] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1961);
  VALUES[1962] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1962);
  VALUES[1963] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1963);
  VALUES[1964] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1964);
  VALUES[1965] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1965);
  VALUES[1966] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1966);
  VALUES[1967] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1967);
  VALUES[1968] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1968);
  VALUES[1969] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1969);
  VALUES[1970] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1970);
  VALUES[1971] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1971);
  VALUES[1972] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1972);
  VALUES[1973] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1973);
  VALUES[1974] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1974);
  VALUES[1975] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1975);
  VALUES[1976] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1976);
  VALUES[1977] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1977);
  VALUES[1978] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1978);
  VALUES[1979] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1979);
  VALUES[1980] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1980);
  VALUES[1981] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1981);
  VALUES[1982] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1982);
  VALUES[1983] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1983);
  VALUES[1984] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1984);
  VALUES[1985] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1985);
  VALUES[1986] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1986);
  VALUES[1987] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1987);
  VALUES[1988] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1988);
  VALUES[1989] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1989);
  VALUES[1990] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1990);
  VALUES[1991] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1991);
  VALUES[1992] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1992);
  VALUES[1993] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1993);
  VALUES[1994] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1994);
  VALUES[1995] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1995);
  VALUES[1996] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1996);
  VALUES[1997] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1997);
  VALUES[1998] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1998);
  VALUES[1999] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_1999);
  VALUES[2000] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2000);
  VALUES[2001] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2001);
  VALUES[2002] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2002);
  VALUES[2003] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2003);
  VALUES[2004] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2004);
  VALUES[2005] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2005);
  VALUES[2006] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2006);
  VALUES[2007] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2007);
  VALUES[2008] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2008);
  VALUES[2009] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2009);
  VALUES[2010] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2010);
  VALUES[2011] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2011);
  VALUES[2012] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2012);
  VALUES[2013] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2013);
  VALUES[2014] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2014);
  VALUES[2015] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2015);
  VALUES[2016] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2016);
  VALUES[2017] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2017);
  VALUES[2018] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2018);
  VALUES[2019] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2019);
  VALUES[2020] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2020);
  VALUES[2021] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2021);
  VALUES[2022] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2022);
  VALUES[2023] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2023);
  VALUES[2024] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2024);
  VALUES[2025] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2025);
  VALUES[2026] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2026);
  VALUES[2027] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2027);
  VALUES[2028] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2028);
  VALUES[2029] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2029);
  VALUES[2030] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2030);
  VALUES[2031] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2031);
  VALUES[2032] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2032);
  VALUES[2033] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2033);
  VALUES[2034] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2034);
  VALUES[2035] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2035);
  VALUES[2036] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2036);
  VALUES[2037] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2037);
  VALUES[2038] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2038);
  VALUES[2039] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2039);
  VALUES[2040] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2040);
  VALUES[2041] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2041);
  VALUES[2042] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2042);
  VALUES[2043] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2043);
  VALUES[2044] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2044);
  VALUES[2045] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2045);
  VALUES[2046] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2046);
  VALUES[2047] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2047);
  VALUES[2048] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2048);
  VALUES[2049] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2049);
  VALUES[2050] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2050);
  VALUES[2051] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2051);
  VALUES[2052] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2052);
  VALUES[2053] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2053);
  VALUES[2054] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2054);
  VALUES[2055] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2055);
  VALUES[2056] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2056);
  VALUES[2057] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2057);
  VALUES[2058] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2058);
  VALUES[2059] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2059);
  VALUES[2060] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2060);
  VALUES[2061] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2061);
  VALUES[2062] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2062);
  VALUES[2063] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2063);
  VALUES[2064] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2064);
  VALUES[2065] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2065);
  VALUES[2066] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2066);
  VALUES[2067] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2067);
  VALUES[2068] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2068);
  VALUES[2069] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2069);
  VALUES[2070] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2070);
  VALUES[2071] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2071);
  VALUES[2072] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2072);
  VALUES[2073] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2073);
  VALUES[2074] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2074);
  VALUES[2075] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2075);
  VALUES[2076] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2076);
  VALUES[2077] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2077);
  VALUES[2078] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2078);
  VALUES[2079] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2079);
  VALUES[2080] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2080);
  VALUES[2081] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2081);
  VALUES[2082] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2082);
  VALUES[2083] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2083);
  VALUES[2084] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2084);
  VALUES[2085] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2085);
  VALUES[2086] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2086);
  VALUES[2087] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2087);
  VALUES[2088] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2088);
  VALUES[2089] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2089);
  VALUES[2090] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2090);
  VALUES[2091] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2091);
  VALUES[2092] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2092);
  VALUES[2093] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2093);
  VALUES[2094] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2094);
  VALUES[2095] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2095);
  VALUES[2096] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2096);
  VALUES[2097] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2097);
  VALUES[2098] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2098);
  VALUES[2099] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2099);
  VALUES[2100] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2100);
  VALUES[2101] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2101);
  VALUES[2102] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2102);
  VALUES[2103] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2103);
  VALUES[2104] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2104);
  VALUES[2105] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2105);
  VALUES[2106] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2106);
  VALUES[2107] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2107);
  VALUES[2108] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2108);
  VALUES[2109] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2109);
  VALUES[2110] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2110);
  VALUES[2111] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2111);
  VALUES[2112] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2112);
  VALUES[2113] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2113);
  VALUES[2114] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2114);
  VALUES[2115] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2115);
  VALUES[2116] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2116);
  VALUES[2117] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2117);
  VALUES[2118] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2118);
  VALUES[2119] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2119);
  VALUES[2120] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2120);
  VALUES[2121] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2121);
  VALUES[2122] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2122);
  VALUES[2123] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2123);
  VALUES[2124] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2124);
  VALUES[2125] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2125);
  VALUES[2126] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2126);
  VALUES[2127] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2127);
  VALUES[2128] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2128);
  VALUES[2129] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2129);
  VALUES[2130] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2130);
  VALUES[2131] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2131);
  VALUES[2132] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2132);
  VALUES[2133] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2133);
  VALUES[2134] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2134);
  VALUES[2135] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2135);
  VALUES[2136] = J_ARRAY_STATIC(PRUnichar, PRInt32, VALUE_2136);

  WINDOWS_1252 = new PRUnichar*[32];
  for (PRInt32 i = 0; i < 32; ++i) {
    WINDOWS_1252[i] = &(WINDOWS_1252_DATA[i]);
  }
}

void
nsHtml5NamedCharacters::releaseStatics()
{
  NAMES.release();
  delete[] VALUES;
  delete[] WINDOWS_1252;
}

