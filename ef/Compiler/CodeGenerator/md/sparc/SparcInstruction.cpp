/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "Fundamentals.h"
#include "SparcInstruction.h"
#include "ControlNodes.h"

SparcInstructionInfo siInfo[nSparcInstructionKind] = 
{
  { siAnd,    2, 0x01, "and"    },
  { siAndCC,  2, 0x11, "andcc"  },
  { siAndN,   2, 0x05, "andn"   },
  { siAndNCC, 2, 0x15, "andncc" },
  { siOr,     2, 0x02, "or"     },
  { siOrCC,   2, 0x12, "orcc"   },
  { siOrN,    2, 0x06, "orn"    },
  { siOrNCC,  2, 0x16, "orncc"  },
  { siXor,    2, 0x03, "xor"    },
  { siXorCC,  2, 0x13, "xorcc"  },
  { siXNor,   2, 0x07, "xnor"   },
  { siXNorCC, 2, 0x17, "xnorcc" },
  { siSll,    2, 0x25, "sll"    },
  { siSrl,    2, 0x26, "srl"    },
  { siSra,    2, 0x27, "sra"    },
  { siAdd,    2, 0x00, "add"    },
  { siAddCC,  2, 0x10, "addcc"  },
  { siAddX,   2, 0x08, "addx"   },
  { siAddXCC, 2, 0x18, "addxcc" },
  { siSub,    2, 0x04, "sub"    },
  { siSubCC,  2, 0x14, "subcc"  },
  { siSubX,   2, 0x0c, "subx"   },
  { siSubXCC, 2, 0x1c, "subxcc" },
  { siUMul,   2, 0x0a, "umul"   },
  { siUMulCC, 2, 0x1a, "umulcc" },
  { siSMul,   2, 0x0b, "smul"   },
  { siSMulCC, 2, 0x1b, "smulcc" },
  { siUDiv,   2, 0x0e, "udiv"   },
  { siUDivCC, 2, 0x1e, "udivcc" },
  { siSDiv,   2, 0x0f, "sdiv"   },
  { siSDivCC, 2, 0x1f, "sdvicc" },
  { siTrap,   2, 0x3a, "trap"   },
  { siLdSb,   3, 0x09, "ldsb"   },
  { siLdSh,   3, 0x0a, "ldsh"   },
  { siLdUb,   3, 0x01, "ldub"   },
  { siLdUh,   3, 0x02, "lduh"   },
  { siLd,     3, 0x00, "ld"     },
  { siLdd,    3, 0x03, "ldd"    },
  { siStSb,   3, 0x05, "stsb"   },
  { siStSh,   3, 0x06, "stsh"   },
  { siSt,     3, 0x04, "st"     },
  { siStd,    3, 0x07, "std"    },
  { siBranch, 0, 0x02, "b"      },
};

SparcAccessInfo saInfo[nSparcAccessKind] =
{
  { saRRR, 2, 1 },
  { saRIR, 1, 1 },
  { saRZR, 1, 1 },
  { saZRR, 1, 1 },
  { saZIR, 0, 1 },
  { saZZR, 0, 1 },
  { saRRZ, 2, 0 },
  { saRIZ, 1, 0 },
  { saRZZ, 1, 0 },
};

SparcConditionInfo scInfo[nSparcConditionKind] =
{
  { scA,   0x08, "a"   },
  { scN,   0x00, "n"   },
  { scNe,  0x09, "ne"  },
  { scE,   0x01, "e"   },
  { scG,   0x0a, "g"   },
  { scLe,  0x02, "le"  },
  { scGe,  0x0b, "ge"  },
  { scL,   0x03, "l"   },
  { scGu,  0x0c, "gu"  },
  { scLeu, 0x04, "leu" },
  { scCc,  0x0d, "cc"  },
  { scGeu, 0x0d, "cc"  },
  { scCs,  0x05, "cs"  },
  { scLu,  0x05, "cs"  },
  { scPos, 0x0e, "pos" },
  { scNeg, 0x06, "neg" },
  { scVc,  0x0f, "vc"  },
  { scVs,  0x07, "vs"  }
};

char* registerNumberToString[] =
{
  "g0", "g1", "g2", "g3", "g4", "g5", "g6", "g7",
  "o0", "o1", "o2", "o3", "o4", "o5", "sp", "o7",
  "l0", "l1", "l2", "l3", "l4", "l5", "l6", "l7",
  "i0", "i1", "i2", "i3", "i4", "i5", "fp", "i7"  
};

uint8 registerNumberToColor[] = 
{
  255, 255, 255, 255, 255, 255, 255, 255,
    8,   9,  10,  11,  12,  13, 255, 255,
    0,   1,   2,   3,   4,   5,   6,   7,
   14,  15,  16,  17,  18,  19, 255, 255
};

SparcRegisterNumber colorToRegisterNumber[] =
{
  l0, l1, l2, l3, l4, l5, l6, l7,
  o0, o1, o2, o3, o4, o5,
  i0, i1, i2, i3, i4, i5,
};

void SparcFormatBCond::
formatToMemory(void* inStart, uint32 inOffset)
{
  uint32 insn = 0;
  insn |= (siInfo[kind].op & 0x3) << 30;
  if (a) insn |= (1 << 29);
  insn |= (scInfo[cond].cond & 0xf) << 25;
  insn |= (siInfo[kind].op3 & 0x7) << 22;
  insn |= ((target.getNativeOffset() - inOffset) >> 2) & 0x3fffff;
  *(uint32*) inStart = insn;
  if (a) ((uint32*) inStart)[1] = 1 << 24;
}

void SparcFormatC::
formatToMemory(void* inStart, uint32 /*inOffset*/)
{
  uint32 insn = 0;

  insn |= (siInfo[kind].op & 0x3) << 30;
  insn |= (getRD() & 0x1f) << 25;
  insn |= (siInfo[kind].op3 & 0x3f) << 19;
  insn |= (getRS1() & 0x1f) << 14;
  if (imm13)
	insn |= (imm13 | 0x2000);
  else
	insn |= getRS2();
	
  *(uint32*) inStart = insn;
}


void SparcFormatC::
printPretty(FILE* f)
{
  switch (access)
	{
	case saRIR: case saZIR: case saRIZ:
	  fprintf(f, "%s %%%s,%d,%%%s", siInfo[kind].string, 
			  registerNumberToString[getRS1()],
			  imm13,
			  registerNumberToString[getRD()]);
	  break;
	default:
	  fprintf(f, "%s %%%s,%%%s,%%%s", siInfo[kind].string, 
			  registerNumberToString[getRS1()],
			  registerNumberToString[getRS2()],
			  registerNumberToString[getRD()]);
	}
}

void SparcLoadStore::
printPretty(FILE* f)
{
  switch (access)
	{
	case saRIR: case saRZR:
	  if (isLoad)
		fprintf(f, "%s [%%%s+%d],%%%s", siInfo[kind].string,
				registerNumberToString[getRS1()],
				imm13,
				registerNumberToString[getRD()]);
	  else
		fprintf(f, "%s %%%s,[%%%s+%d]", siInfo[kind].string,
				registerNumberToString[getRD()],
				registerNumberToString[getRS1()],
				imm13);
	  break;
	default:
	  if (isLoad)
		fprintf(f, "%s [%%%s+%%%s],%%%s", siInfo[kind].string,
				registerNumberToString[getRS1()],
				registerNumberToString[getRS2()],
				registerNumberToString[getRD()]);
	  else
		fprintf(f, "%s %%%s,[%%%s+%%%s]", siInfo[kind].string,
				registerNumberToString[getRD()],
				registerNumberToString[getRS1()],
				registerNumberToString[getRS2()]);
	  break;
	}
}

void SparcTrap::
printPretty(FILE* f)
{
  fprintf(f, "t%s throw_exception", scInfo[cond].string);
}

void SparcFormatBCond::
printPretty(FILE* f)
{
  fprintf(f, "%s%s%s N%d%s", siInfo[kind].string, scInfo[cond].string, a ? ",a" : "", 
		  target.dfsNum, a ? "; nop" : "");
}

uint8 SparcFormatC::
getRS1()
{
  switch (access)
	{
	case saZRR: case saZIR: case saZZR:
	  return g0;

	default:
	  return useToRegisterNumber(getInstructionUseBegin()[0]);
	}
}

uint8 SparcFormatC::
getRS2()
{
#if DEBUG
  if (access == saRIR || access == saZIR || access == saRIZ) assert(0);
#endif

  switch (access)
	{
	case saZZR: case saRZZ: case saRZR:
	  return g0;

	default:
	  return useToRegisterNumber(getInstructionUseBegin()[1]);
	}
}

uint8 SparcFormatC::
getRD()
{
  switch (access)
	{
	case saRRZ: case saRIZ: case saRZZ:
	  return g0;

	default:
	  return defineToRegisterNumber(getInstructionDefineBegin()[0]);
	}
}

uint8 SparcLoadStore::
getRS1()
{
  switch (access)
	{
	case saZRR: case saZIR: case saZZR:
	  return g0;

	default:
	  return useToRegisterNumber(getInstructionUseBegin()[isLoadC ? 0 : 1]);
	}
}

uint8 SparcLoadStore::
getRS2()
{
  assert(0);
}

uint8 SparcLoadStore::
getRD()
{
  switch (access)
	{
	case saRRZ: case saRIZ: case saRZZ:
	  return g0;

	default:
	  return (isLoad) ? defineToRegisterNumber(getInstructionDefineBegin()[hasMemoryEdge ? 1 : 0]) : 
		useToRegisterNumber(getInstructionUseBegin()[2]);
	}
}
