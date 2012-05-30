/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
typedef short int16;
typedef unsigned short uint16;

#define NOMAPPING 0xfffd

typedef struct {
		uint16 srcBegin;		/* 2 byte	*/
		uint16 srcEnd;			/* 2 byte	*/
		uint16 destBegin;		/* 2 byte	*/
} uFormat0;

typedef struct {
		uint16 srcBegin;		/* 2 byte	*/
		uint16 srcEnd;			/* 2 byte	*/
		uint16	mappingOffset;	/* 2 byte	*/
} uFormat1;

typedef struct {
		uint16 srcBegin;		/* 2 byte	*/
		uint16 srcEnd;			/* 2 byte	-waste	*/
		uint16 destBegin;		/* 2 byte	*/
} uFormat2;

typedef struct  {
	union {
		uFormat0	format0;
		uFormat1	format1;
		uFormat2	format2;
	} fmt;
} uMapCell;

/* =================================================
					uTable 
================================================= */
typedef struct  {
	uint16 		itemOfList;
	uint16		offsetToFormatArray;
	uint16		offsetToMapCellArray;
	uint16		offsetToMappingTable;
	uint16		data[1];
} uTable;

uint16 umap[256][256];
int bInitFromOrTo = 0;
int bGenerateFromUnicodeTable = 0;

#define MAXCELLNUM 1000

static int numOfItem = 0;
uMapCell cell[MAXCELLNUM];
uint16    format[MAXCELLNUM / 4];
uint16   mapping[256*256];
static int mappinglen  = 0;
static int formatcount[4] = {0,0,0,0}; 

#define SetFormat(n,f)		{ format[(n >> 2)] |= ((f) << ((n & 0x0003)	<< 2)); formatcount[f]++; }
#define GetFormat(n)		( format[(n >> 2)] >> ((n & 0x0003)	<< 2)) &0x00FF)
#define MAPVALUE(i)	(umap[(i >> 8) & 0xFF][(i) & 0xFF])

int  FORMAT1CNST = 10 ;
int  FORMAT0CNST = 5 ;
void initmaps()
{
	int i,j;
	for(i=0;i<256;i++)
		for(j=0;j<256;j++) 
		{
			umap[i][j]=   NOMAPPING;
		}
	for(i=0;i<MAXCELLNUM / 4;i++)
		format[i]=0;
}
void SetMapValue(short u,short c)
{
        if(NOMAPPING == MAPVALUE(u))
	   MAPVALUE(u) = c & 0x0000FFFF;
        else {
           fprintf(stderr, "warning- duplicate mapping %x map to both %x and %x\n", u, MAPVALUE(u), c);
        }
}
void AddFormat2(uint16 srcBegin)
{
	uint16 destBegin = MAPVALUE(srcBegin);
	printf("Begin of Item %04X\n",numOfItem);
	printf(" Format 2\n");
	printf("  srcBegin = %04X\n", srcBegin);
	printf("  destBegin = %04X\n", destBegin );
	SetFormat(numOfItem,2);
	cell[numOfItem].fmt.format2.srcBegin = srcBegin;
	cell[numOfItem].fmt.format2.srcEnd = 0;
	cell[numOfItem].fmt.format2.destBegin = destBegin;
	printf("End of Item %04X \n\n",numOfItem);
	numOfItem++;
	/*	Unmark the umap */
	MAPVALUE(srcBegin) = NOMAPPING;
}
void AddFormat1(uint16 srcBegin, uint16 srcEnd)
{
	uint16 i;
	printf("Begin of Item %04X\n",numOfItem);
	printf(" Format 1\n");
	printf("  srcBegin = %04X\n", srcBegin);
	printf("  srcEnd = %04X\n", srcEnd );
	printf("  mappingOffset = %04X\n", mappinglen);
	printf(" Mapping  = " );  
	SetFormat(numOfItem,1);
	cell[numOfItem].fmt.format1.srcBegin = srcBegin;
	cell[numOfItem].fmt.format1.srcEnd = srcEnd;
	cell[numOfItem].fmt.format1.mappingOffset = mappinglen;
	for(i=srcBegin ; i <= srcEnd ; i++,mappinglen++)
	{
		if( ((i-srcBegin) % 8) == 0)
			printf("\n  ");
		mapping[mappinglen]= MAPVALUE(i);
		printf("%04X ",(mapping[mappinglen]  ));
		/*	Unmark the umap */
		MAPVALUE(i) = NOMAPPING;
	}
	printf("\n");
	printf("End of Item %04X \n\n",numOfItem);
	numOfItem++;
}
void AddFormat0(uint16 srcBegin, uint16 srcEnd)
{
	uint16 i;
	uint16 destBegin = MAPVALUE(srcBegin);
	printf("Begin of Item %04X\n",numOfItem);
	printf(" Format 0\n");
	printf("  srcBegin = %04X\n", srcBegin);
	printf("  srcEnd = %04X\n", srcEnd );
	printf("  destBegin = %04X\n", destBegin );
	SetFormat(numOfItem,0);
	cell[numOfItem].fmt.format0.srcBegin = srcBegin;
	cell[numOfItem].fmt.format0.srcEnd = srcEnd;
	cell[numOfItem].fmt.format0.destBegin = destBegin;
	for(i=srcBegin ; i <= srcEnd ; i++)
	{
		/*	Unmark the umap */
		MAPVALUE(i) = NOMAPPING;
	}
	printf("End of Item %04X \n\n",numOfItem);
	numOfItem++;
}
void printnpl()
{
printf(
"/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */\n"
"/* This Source Code Form is subject to the terms of the Mozilla Public\n"
" * License, v. 2.0. If a copy of the MPL was not distributed with this\n"
" * file, You can obtain one at http://mozilla.org/MPL/2.0/. */\n");
}
void gentable()
{
	/*	OK! For now, we just use format 1 for each row */
	/*	We need to chage this to use other format to save the space */
	uint16 begin,end;
	uint16 ss,gs,gp,state,gc;	
	uint16 diff, lastdiff;

        printnpl();
	printf("/*========================================================\n");
	printf("  This is a Generated file. Please don't edit it.\n");
	printf("\n");
	printf("  The tool which used to generate this file is called umaptable.\n");
	printf("  You can find this tool under mozilla/intl/uconv/tools/umaptable.c.\n");

	printf("  If you have any problem of this file. Please contact \n"); 
	printf("  Netscape Client International Team or \n");
	printf("  ftang@netscape <Frank Tang> \n");
	printf("\n");
	printf("              Table in Debug form \n");

	for(begin = 0; MAPVALUE(begin) ==NOMAPPING; begin++)
		;
	for(end = 0xFFFF; MAPVALUE(end) ==NOMAPPING; end--)
		;
	if(end != begin)
	{
	   lastdiff = MAPVALUE(begin) - begin; 
		for(gp=begin+1,state = 0 ; gp<=end; gp++)
		{
			int input ;
	   	diff = MAPVALUE(gp) - gp; 
		   input = (diff == lastdiff);
			switch(state)
			{
			 	case 0:	
					if(input)
					{
						state = 1;
					   ss =  gp -1;
					   gc = 2;
					}
					break;
			   case 1:
					if(input)
					{
						if(gc++ >= FORMAT0CNST)
						{
							state = 2;
						}
					}
					else
					{
						state = 0;
					}
					break;
			   case 2:
					if(input)
					{
					}
					else
					{
					   AddFormat0(ss,gp-1);
						state = 0;
					}
					break;
			}
			
		   lastdiff = diff;
		}	
	}
	if(state == 2)
		AddFormat0(ss,end);

	for(;(MAPVALUE(begin) ==NOMAPPING) && (begin <= end); begin++)
		;
 if(begin <= end)
 {
		for(;(MAPVALUE(end)==NOMAPPING) && (end >= begin); end--)
			;
		for(ss=gp=begin,state = 0 ; gp<=end; gp++)
		{
			int input = (MAPVALUE(gp) == NOMAPPING);
			switch(state)
			{
			case 0:
				if(input)
				{
					gc = 1;
					gs = gp;
					state = 1;
			}
				break;
			case 1:
				if(input)
				{
					if(gc++ >= FORMAT1CNST)
						state = 2;
				}
				else		
					state = 0;
				break;
			case 2:
				if(input)
				{		
				}
				else
				{
			   	if(gs == (ss+1))
						AddFormat2(ss);	
					else
						AddFormat1(ss ,gs-1);	
					state = 0;
					ss = gp;
				}
						break;
					}
				}
				if(end == ss)
					AddFormat2(ss );	
				else
					AddFormat1(ss ,end );	
	}
	printf("========================================================*/\n");
}
void writetable()
{
	uint16 i;
	uint16 off1,off2,off3;
	uint16 cur = 0; 
	uint16 formatitem = (((numOfItem)>>2) + 1);
	off1 = 4;
	off2 = off1 + formatitem ;
	off3 = off2 + numOfItem * sizeof(uMapCell) / sizeof(uint16);
	/*	write itemOfList		*/
	printf("/* Offset=0x%04X  ItemOfList */\n  0x%04X,\n", cur++, numOfItem);

	/*	write offsetToFormatArray	*/
	printf("/*-------------------------------------------------------*/\n");
	printf("/* Offset=0x%04X  offsetToFormatArray */\n  0x%04X,\n",  cur++,off1);

	/*	write offsetToMapCellArray	*/
	printf("/*-------------------------------------------------------*/\n");
	printf("/* Offset=0x%04X  offsetToMapCellArray */ \n  0x%04X,\n",  cur++,off2);

	/*	write offsetToMappingTable	*/
	printf("/*-------------------------------------------------------*/\n");
	printf("/* Offset=0x%04X  offsetToMappingTable */ \n  0x%04X,\n", cur++,off3);

	/*	write FormatArray		*/
	printf("/*-------------------------------------------------------*/\n");
	printf("/*       Offset=0x%04X   Start of Format Array */ \n",cur);
	printf("/*	Total of Format 0 : 0x%04X			 */\n"
			, formatcount[0]);	
	printf("/*	Total of Format 1 : 0x%04X			 */\n"
			, formatcount[1]);	
	printf("/*	Total of Format 2 : 0x%04X			 */\n"
			, formatcount[2]);	
	printf("/*	Total of Format 3 : 0x%04X			 */\n"
			, formatcount[3]);	
	for(i=0;i<formatitem;i++,cur++)
	{
		if((i%8) == 0)	
			printf("\n");
		printf("0x%04X, ",format[i]);
	}
	printf("\n");

	/*	write MapCellArray		*/
	printf("/*-------------------------------------------------------*/\n");
	printf("/*       Offset=0x%04X   Start of MapCell Array */ \n",cur);
	for(i=0;i<numOfItem;i++,cur+=3)
	{
		printf("/* %04X */    0x%04X, 0x%04X, 0x%04X, \n", 
			i,
			cell[i].fmt.format0.srcBegin,
			cell[i].fmt.format0.srcEnd,
			cell[i].fmt.format0.destBegin
	        );
	}

	/*	write MappingTable		*/
	printf("/*-------------------------------------------------------*/\n");
	printf("/*       Offset=0x%04X   Start of MappingTable */ \n",cur);
	for(i=0;i<mappinglen;i++,cur++)
	{
		if((i%8) == 0)	
			printf("\n/* %04X */    ",i);
		printf("0x%04X, ",mapping[i] );
	}
	printf("\n");
	printf("/*	End of table Total Length = 0x%04X * 2 */\n",cur);
}

void usage()
{
  fprintf(stderr, "please indicate what kind of mapping mapping table you want to generate:\n");
  fprintf(stderr, "\t-uf : generate *.uf (from unicode) table, or\n");
  fprintf(stderr, "\t-ut : generate *.ut (to unicode) table\n");
}
parsearg(int argc, char* argv[])
{
	int i;
	for(i=0;i<argc;i++)
	{
		if(strncmp("-uf", argv[i],3) == 0) {
                        if(! bInitFromOrTo) {
                           bGenerateFromUnicodeTable = 1;
                           bInitFromOrTo = 1;
                        } else {
                           usage();
                           exit(-1);
                        }
                } 
		if(strncmp("-ut", argv[i],3) == 0) {
                        if(! bInitFromOrTo) {
                           bGenerateFromUnicodeTable = 0;
                           bInitFromOrTo = 1;
                        } else {
                           usage();
                           exit(-1);
                        }
                } 
		if((strncmp("-0", argv[i],2) == 0) && ((i+1) < argc))
		{
			int cnst0;
			if(sscanf(argv[i+1], "%d", &cnst0) == 1)
			{
				if(cnst0 > 0)
				{
					FORMAT0CNST = cnst0;
				}
			}
			else
			{
				fprintf(stderr, "argc error !!!!\n");
				exit(-1);
			}
			i++;
		}
		if((strncmp("-1", argv[i],2) == 0) && ((i+1) < argc))
		{
			int cnst1;
			if(sscanf(argv[i+1], "%d", &cnst1) == 1)
			{
				if(cnst1 > 0)
				{
					FORMAT1CNST = cnst1;
				}
			}
			else
			{
				fprintf(stderr, "argc error !!!!\n");
				exit(-1);
			}
			i++;
		}
	}
        if(! bInitFromOrTo)
        {
                usage();
		exit(-1);
        }
	fprintf(stderr, "format 0 cnst = %d\n", FORMAT0CNST);
	fprintf(stderr, "format 1 cnst = %d\n", FORMAT1CNST);
        fprintf(stderr, "generate u%c table\n", 
                        bGenerateFromUnicodeTable ? 'f' : 't');
}
void getinput()
{
  char buf[256];
  short c,u;
  for(;gets(buf)!=NULL;)
  {
     if(buf[0]=='0' && buf[1] == 'x')
        {
          sscanf(buf,"%hx %hx",&c,&u);
          if(bGenerateFromUnicodeTable)
            SetMapValue(u, c);
          else
            SetMapValue(c, u);
        }
  }
}
main(int argc, char* argv[])
{
  parsearg(argc, argv);
  initmaps();
  getinput();
  gentable();
  writetable();	
}
