/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "xp.h"
#include "layout.h"

#ifdef PROFILE
#pragma profile on
#endif

PA_Block
lo_ValueToAlpha(int32 value, Bool large, intn *len_ptr)
{
	int i;
	char str[20];
	char str2[22];
	intn pos, cnt;
	PA_Block buff;
	char *bptr;
	char base;

	*len_ptr = 0;

	if (large != FALSE)
	{
		base = 'A';
	}
	else
	{
		base = 'a';
	}

	for (i=0; i<20; i++)
	{
		str[i] = (char)0;
	}

	while (value > 26)
	{
		pos = 1;
		str[pos] = (char)((int)str[pos] + 1);
		cnt = (int)str[pos];
		while ((cnt > 26)&&(pos < 19))
		{
			str[pos] = (char)0;
			pos++;
			str[pos] = (char)((int)str[pos] + 1);
			cnt = (int)str[pos];
		}
		if ((pos == 20)&&(cnt > 26))
		{
			str[pos] = (char)0;
		}
		value -= 26;
	}
	str[0] = (char)value;

	pos = 0;
	while ((int)str[pos] != 0)
	{
		pos++;
	}

	if (pos == 0)
	{
		XP_STRCPY(str2, " .");
	}
	else
	{
		cnt = 0;
		for (i=pos; i>0; i--)
		{
			str2[cnt] = (char)(base + (int)str[i - 1] - 1);
			cnt++;
		}
		str2[cnt] = '.';
		str2[cnt + 1] = '\0';
	}

	*len_ptr = XP_STRLEN(str2);

	buff = PA_ALLOC(*len_ptr + 1);
	if (buff != NULL)
	{
		PA_LOCK(bptr, char *, buff);
		XP_STRCPY(bptr, str2);
		PA_UNLOCK(buff);
	}

	return(buff);
}


static char Fives[2][5] = {"vld ", "VLD "};
static char Ones[2][5] = {"ixcm", "IXCM"};

PA_Block
lo_ValueToRoman(int32 value, Bool large, intn *len_ptr)
{
	int i, j;
	int indx[4];
	char str[4][6];
	char *fives;
	char *ones;
	char str2[22];
	char *ptr;
	PA_Block buff;
	char *bptr;

	*len_ptr = 0;

	if (large != FALSE)
	{
		fives = Fives[1];
		ones = Ones[1];
	}
	else
	{
		fives = Fives[0];
		ones = Ones[0];
	}

	if (value >= 4000)
	{
		value = value % 3999;
		value++;
	}

	for (i=0; i<4; i++)
	{
		indx[i] = (int) value % 10;
		value  = value / 10;
	}

	for (i=0; i<4; i++)
	{
		if (indx[i] >= 5)
		{
			indx[i] -= 5;
			str[i][0] = fives[i];
		}
		else
		{
			str[i][0] = ' ';
		}

		if (indx[i] == 4)
		{
			if (str[i][0] == ' ')
			{
				str[i][1] = fives[i];
			}
			else
			{
				str[i][1] = ones[i + 1];
			}
			str[i][0] = ones[i];
			str[i][2] = '\0';
		}
		else
		{
			for (j=0; j<indx[i]; j++)
			{
				str[i][j + 1] = ones[i];
			}
			str[i][indx[i] + 1] = '\0';
		}
	}

	XP_STRCPY(str2, "");
	for (i=3; i>=0; i--)
	{
		ptr = str[i];
		if (*ptr == ' ')
		{
			ptr++;
		}
		XP_STRCAT(str2, ptr);
	}
	XP_STRCAT(str2, ".");

	*len_ptr = XP_STRLEN(str2);

	buff = PA_ALLOC(*len_ptr + 1);
	if (buff != NULL)
	{
		PA_LOCK(bptr, char *, buff);
		XP_STRCPY(bptr, str2);
		PA_UNLOCK(buff);
	}

	return(buff);
}

#ifdef PROFILE
#pragma profile off
#endif
