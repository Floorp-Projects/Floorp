/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef SS_HEADER
#define SS_HEADER

/* maxint */
#define MAX_STYLESTRUCT_PRIORITY (((unsigned) (~0) << 1) >> 1)

#define STYLESTRUCT_NewSSNumber(self, num, uts) (*self->vtable->styleNewSSNumber)(self, num, uts)
#define STYLESTRUCT_FreeSSNumber(self, num)		(*self->vtable->styleFreeSSNumber)(self, num)
#define STYLESTRUCT_CopySSNumber(self, num)		(*self->vtable->styleCopySSNumber)(self, num)
#define STYLESTRUCT_StringToSSNumber(self, strng) (*self->vtable->styleStringToSSNumber)(self, strng)
#define STYLESTRUCT_SetString(self, name, value, priority)	\
											(*self->vtable->styleSetString)(self, name, value, priority)
#define STYLESTRUCT_SetNumber(self, name, num, priority)	(*self->vtable->styleSetNumber)(self, name, num, priority)
#define STYLESTRUCT_GetString(self, name)		(*self->vtable->styleGetString)(self, name)
#define STYLESTRUCT_GetNumber(self, name)		(*self->vtable->styleGetNumber)(self, name)
#define STYLESTRUCT_Count(self)					(*self->vtable->styleCount)(self)
#define STYLESTRUCT_Duplicate(self)				(*self->vtable->styleDuplicate)(self)
#define STYLESTRUCT_Delete(self)				(*self->vtable->styleDelete)(self)


typedef struct _SS_Number {
	double value;
	char  *units;
} SS_Number;

typedef struct _StyleStructInterface StyleStructInterface;

typedef struct _StyleStruct {
    StyleStructInterface* vtable;
    int32    refcount;
} StyleStruct;

struct _StyleStructInterface {
    SS_Number* (*styleNewSSNumber)(StyleStruct *self, double value, char *units);
    void (*styleFreeSSNumber)(StyleStruct * self, SS_Number *obj);
    SS_Number* (*styleCopySSNumber)(StyleStruct *self, SS_Number *old_num);
	SS_Number* (*styleStringToSSNumber)(StyleStruct *self, char *strng);
    void (*styleSetString)(StyleStruct * self, char *name, char *value, int32 priority);
    void (*styleSetNumber)(StyleStruct * self, char *name, SS_Number *value, int32 priority);
    char* (*styleGetString)(StyleStruct * self, char *name);
    SS_Number* (*styleGetNumber)(StyleStruct * self, char *name);
    uint32 (*styleCount)(StyleStruct * self);
    StyleStruct* (*styleDuplicate)(StyleStruct * self);
    void (*styleDelete)(StyleStruct * self);
};


/* initializer */
extern StyleStruct * STYLESTRUCT_Factory_Create(void);

#endif /* SS_HEADER */
