/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is PPEmbed code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Conrad Carlen <ccarlen@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <Types.r>
#include <AEUserTermTypes.r> 

resource 'aete' (0, "English") {
	0x1,
	0x0,
	english,
	roman,
	{	/* array Suites: 4 elements */
		/* [1] */
		"Required Suite",
		"Terms that every application should supp"
		"ort",
		'reqd',
		1,
		1,
		{	/* array Events: 0 elements */
		},
		{	/* array Classes: 0 elements */
		},
		{	/* array ComparisonOps: 0 elements */
		},
		{	/* array Enumerations: 0 elements */
		},
		/* [2] */
		"Standard Suite",
		"Common terms for most applications",
		'CoRe',
		1,
		1,
		{	/* array Events: 8 elements */
			/* [1] */
			"close",
			"Close an object",
			'core',
			'clos',
			noReply,
			"",
			replyOptional,
			singleItem,
			notEnumerated,
			notTightBindingFunction,
			enumsAreConstants,
			enumListCanRepeat,
			replyIsValue,
			reserved,
			reserved,
			reserved,
			reserved,
			reserved,
			verbEvent,
			reserved,
			reserved,
			reserved,
			'obj ',
			"the objects to close",
			directParamRequired,
			singleItem,
			notEnumerated,
			changesState,
			enumsAreConstants,
			enumListCanRepeat,
			directParamIsValue,
			directParamIsTarget,
			reserved,
			reserved,
			reserved,
			reserved,
			reserved,
			reserved,
			reserved,
			reserved,
			{	/* array OtherParams: 2 elements */
				/* [1] */
				"saving",
				'savo',
				'savo',
				"specifies whether or not changes should "
				"be saved before closing",
				optional,
				singleItem,
				enumerated,
				reserved,
				enumsAreConstants,
				enumListCanRepeat,
				paramIsValue,
				notParamIsTarget,
				reserved,
				reserved,
				reserved,
				reserved,
				prepositionParam,
				notFeminine,
				notMasculine,
				singular,
				/* [2] */
				"in",
				'kfil',
				'alis',
				"the file in which to save the object",
				optional,
				singleItem,
				notEnumerated,
				reserved,
				enumsAreConstants,
				enumListCanRepeat,
				paramIsValue,
				notParamIsTarget,
				reserved,
				reserved,
				reserved,
				reserved,
				prepositionParam,
				notFeminine,
				notMasculine,
				singular
			},
			/* [2] */
			"data size",
			"Return the size in bytes of an object",
			'core',
			'dsiz',
			'long',
			"the size of the object in bytes",
			replyRequired,
			singleItem,
			notEnumerated,
			notTightBindingFunction,
			enumsAreConstants,
			enumListCanRepeat,
			replyIsValue,
			reserved,
			reserved,
			reserved,
			reserved,
			reserved,
			verbEvent,
			reserved,
			reserved,
			reserved,
			'obj ',
			"the object whose data size is to be retu"
			"rned",
			directParamRequired,
			singleItem,
			notEnumerated,
			doesntChangeState,
			enumsAreConstants,
			enumListCanRepeat,
			directParamIsValue,
			directParamIsTarget,
			reserved,
			reserved,
			reserved,
			reserved,
			reserved,
			reserved,
			reserved,
			reserved,
			{	/* array OtherParams: 0 elements */
			},
			/* [3] */
			"get",
			"Get the data for an object",
			'core',
			'getd',
			'****',
			"The data from the object",
			replyRequired,
			singleItem,
			notEnumerated,
			notTightBindingFunction,
			enumsAreConstants,
			enumListCanRepeat,
			replyIsValue,
			reserved,
			reserved,
			reserved,
			reserved,
			reserved,
			verbEvent,
			reserved,
			reserved,
			reserved,
			'obj ',
			"the object whose data is to be returned",
			directParamRequired,
			singleItem,
			notEnumerated,
			doesntChangeState,
			enumsAreConstants,
			enumListCanRepeat,
			directParamIsValue,
			directParamIsTarget,
			reserved,
			reserved,
			reserved,
			reserved,
			reserved,
			reserved,
			reserved,
			reserved,
			{	/* array OtherParams: 0 elements */
			},
			/* [4] */
			"make",
			"Make a new element",
			'core',
			'crel',
			'obj ',
			"Object specifier for the new element",
			replyRequired,
			singleItem,
			notEnumerated,
			notTightBindingFunction,
			enumsAreConstants,
			enumListCanRepeat,
			replyIsValue,
			reserved,
			reserved,
			reserved,
			reserved,
			reserved,
			verbEvent,
			reserved,
			reserved,
			reserved,
			noParams,
			"",
			directParamOptional,
			singleItem,
			notEnumerated,
			changesState,
			enumsAreConstants,
			enumListCanRepeat,
			directParamIsValue,
			directParamIsTarget,
			reserved,
			reserved,
			reserved,
			reserved,
			reserved,
			reserved,
			reserved,
			reserved,
			{	/* array OtherParams: 4 elements */
				/* [1] */
				"new",
				'kocl',
				'type',
				"the class of the new element",
				required,
				singleItem,
				notEnumerated,
				reserved,
				enumsAreConstants,
				enumListCanRepeat,
				paramIsValue,
				notParamIsTarget,
				reserved,
				reserved,
				reserved,
				reserved,
				prepositionParam,
				notFeminine,
				notMasculine,
				singular,
				/* [2] */
				"at",
				'insh',
				'insl',
				"the location at which to insert the elem"
				"ent",
				optional,
				singleItem,
				notEnumerated,
				reserved,
				enumsAreConstants,
				enumListCanRepeat,
				paramIsValue,
				notParamIsTarget,
				reserved,
				reserved,
				reserved,
				reserved,
				prepositionParam,
				notFeminine,
				notMasculine,
				singular,
				/* [3] */
				"with data",
				'data',
				'****',
				"the initial data for the element",
				optional,
				singleItem,
				notEnumerated,
				reserved,
				enumsAreConstants,
				enumListCanRepeat,
				paramIsValue,
				notParamIsTarget,
				reserved,
				reserved,
				reserved,
				reserved,
				prepositionParam,
				notFeminine,
				notMasculine,
				singular,
				/* [4] */
				"with properties",
				'prdt',
				'reco',
				"the initial values for the properties of"
				" the element",
				optional,
				singleItem,
				notEnumerated,
				reserved,
				enumsAreConstants,
				enumListCanRepeat,
				paramIsValue,
				notParamIsTarget,
				reserved,
				reserved,
				reserved,
				reserved,
				prepositionParam,
				notFeminine,
				notMasculine,
				singular
			},
			/* [5] */
			"open",
			"Open the specified object(s)",
			'aevt',
			'odoc',
			noReply,
			"",
			replyOptional,
			singleItem,
			notEnumerated,
			notTightBindingFunction,
			enumsAreConstants,
			enumListCanRepeat,
			replyIsValue,
			reserved,
			reserved,
			reserved,
			reserved,
			reserved,
			verbEvent,
			reserved,
			reserved,
			reserved,
			'obj ',
			"Objects to open. Can be a list of files "
			"or an object specifier.",
			directParamRequired,
			singleItem,
			notEnumerated,
			changesState,
			enumsAreConstants,
			enumListCanRepeat,
			directParamIsValue,
			directParamIsTarget,
			reserved,
			reserved,
			reserved,
			reserved,
			reserved,
			reserved,
			reserved,
			reserved,
			{	/* array OtherParams: 0 elements */
			},
			/* [6] */
			"print",
			"Print the specified object(s)",
			'aevt',
			'pdoc',
			noReply,
			"",
			replyOptional,
			singleItem,
			notEnumerated,
			notTightBindingFunction,
			enumsAreConstants,
			enumListCanRepeat,
			replyIsValue,
			reserved,
			reserved,
			reserved,
			reserved,
			reserved,
			verbEvent,
			reserved,
			reserved,
			reserved,
			'obj ',
			"Objects to print. Can be a list of files"
			" or an object specifier.",
			directParamRequired,
			singleItem,
			notEnumerated,
			doesntChangeState,
			enumsAreConstants,
			enumListCanRepeat,
			directParamIsValue,
			directParamIsTarget,
			reserved,
			reserved,
			reserved,
			reserved,
			reserved,
			reserved,
			reserved,
			reserved,
			{	/* array OtherParams: 0 elements */
			},
			/* [7] */
			"save",
			"save a set of objects",
			'core',
			'save',
			noReply,
			"",
			replyRequired,
			singleItem,
			notEnumerated,
			notTightBindingFunction,
			enumsAreConstants,
			enumListCanRepeat,
			replyIsValue,
			reserved,
			reserved,
			reserved,
			reserved,
			reserved,
			verbEvent,
			reserved,
			reserved,
			reserved,
			'obj ',
			"Objects to save.",
			directParamRequired,
			singleItem,
			notEnumerated,
			doesntChangeState,
			enumsAreConstants,
			enumListCanRepeat,
			directParamIsValue,
			directParamIsTarget,
			reserved,
			reserved,
			reserved,
			reserved,
			reserved,
			reserved,
			reserved,
			reserved,
			{	/* array OtherParams: 2 elements */
				/* [1] */
				"in",
				'kfil',
				'alis',
				"the file in which to save the object(s)",
				optional,
				singleItem,
				notEnumerated,
				reserved,
				enumsAreConstants,
				enumListCanRepeat,
				paramIsValue,
				notParamIsTarget,
				reserved,
				reserved,
				reserved,
				reserved,
				prepositionParam,
				notFeminine,
				notMasculine,
				singular,
				/* [2] */
				"as",
				'fltp',
				'type',
				"the file type of the document in which t"
				"o save the data",
				optional,
				singleItem,
				notEnumerated,
				reserved,
				enumsAreConstants,
				enumListCanRepeat,
				paramIsValue,
				notParamIsTarget,
				reserved,
				reserved,
				reserved,
				reserved,
				prepositionParam,
				notFeminine,
				notMasculine,
				singular
			},
			/* [8] */
			"set",
			"Set an object’s data",
			'core',
			'setd',
			noReply,
			"",
			replyOptional,
			singleItem,
			notEnumerated,
			notTightBindingFunction,
			enumsAreConstants,
			enumListCanRepeat,
			replyIsValue,
			reserved,
			reserved,
			reserved,
			reserved,
			reserved,
			verbEvent,
			reserved,
			reserved,
			reserved,
			'obj ',
			"the object to change",
			directParamRequired,
			singleItem,
			notEnumerated,
			changesState,
			enumsAreConstants,
			enumListCanRepeat,
			directParamIsValue,
			directParamIsTarget,
			reserved,
			reserved,
			reserved,
			reserved,
			reserved,
			reserved,
			reserved,
			reserved,
			{	/* array OtherParams: 1 elements */
				/* [1] */
				"to",
				'data',
				'****',
				"the new value",
				required,
				singleItem,
				notEnumerated,
				reserved,
				enumsAreConstants,
				enumListCanRepeat,
				paramIsValue,
				notParamIsTarget,
				reserved,
				reserved,
				reserved,
				reserved,
				prepositionParam,
				notFeminine,
				notMasculine,
				singular
			}
		},
		{	/* array Classes: 3 elements */
			/* [1] */
			"application",
			'capp',
			"An application program",
			{	/* array Properties: 0 elements */
			},
			{	/* array Elements: 2 elements */
				/* [1] */
				'cwin',
				{	/* array KeyForms: 3 elements */
					/* [1] */
					'indx',
					/* [2] */
					'name',
					/* [3] */
					'rele'
				},
				/* [2] */
				'docu',
				{	/* array KeyForms: 1 elements */
					/* [1] */
					'name'
				}
			},
			/* [2] */
			"window",
			'cwin',
			"A Window",
			{	/* array Properties: 12 elements */
				/* [1] */
				"bounds",
				'pbnd',
				'qdrt',
				"the boundary rectangle for the window",
				reserved,
				singleItem,
				notEnumerated,
				readWrite,
				enumsAreConstants,
				enumListCanRepeat,
				propertyIsValue,
				reserved,
				reserved,
				reserved,
				reserved,
				reserved,
				noApostrophe,
				notFeminine,
				notMasculine,
				singular,
				/* [2] */
				"closeable",
				'hclb',
				'bool',
				"Does the window have a close box?",
				reserved,
				singleItem,
				notEnumerated,
				readOnly,
				enumsAreConstants,
				enumListCanRepeat,
				propertyIsValue,
				reserved,
				reserved,
				reserved,
				reserved,
				reserved,
				noApostrophe,
				notFeminine,
				notMasculine,
				singular,
				/* [3] */
				"titled",
				'ptit',
				'bool',
				"Does the window have a title bar?",
				reserved,
				singleItem,
				notEnumerated,
				readOnly,
				enumsAreConstants,
				enumListCanRepeat,
				propertyIsValue,
				reserved,
				reserved,
				reserved,
				reserved,
				reserved,
				noApostrophe,
				notFeminine,
				notMasculine,
				singular,
				/* [4] */
				"index",
				'pidx',
				'long',
				"the number of the window",
				reserved,
				singleItem,
				notEnumerated,
				readWrite,
				enumsAreConstants,
				enumListCanRepeat,
				propertyIsValue,
				reserved,
				reserved,
				reserved,
				reserved,
				reserved,
				noApostrophe,
				notFeminine,
				notMasculine,
				singular,
				/* [5] */
				"floating",
				'isfl',
				'bool',
				"Does the window float?",
				reserved,
				singleItem,
				notEnumerated,
				readOnly,
				enumsAreConstants,
				enumListCanRepeat,
				propertyIsValue,
				reserved,
				reserved,
				reserved,
				reserved,
				reserved,
				noApostrophe,
				notFeminine,
				notMasculine,
				singular,
				/* [6] */
				"modal",
				'pmod',
				'bool',
				"Is the window modal?",
				reserved,
				singleItem,
				notEnumerated,
				readOnly,
				enumsAreConstants,
				enumListCanRepeat,
				propertyIsValue,
				reserved,
				reserved,
				reserved,
				reserved,
				reserved,
				noApostrophe,
				notFeminine,
				notMasculine,
				singular,
				/* [7] */
				"resizable",
				'prsz',
				'bool',
				"Is the window resizable?",
				reserved,
				singleItem,
				notEnumerated,
				readOnly,
				enumsAreConstants,
				enumListCanRepeat,
				propertyIsValue,
				reserved,
				reserved,
				reserved,
				reserved,
				reserved,
				noApostrophe,
				notFeminine,
				notMasculine,
				singular,
				/* [8] */
				"zoomable",
				'iszm',
				'bool',
				"Is the window zoomable?",
				reserved,
				singleItem,
				notEnumerated,
				readOnly,
				enumsAreConstants,
				enumListCanRepeat,
				propertyIsValue,
				reserved,
				reserved,
				reserved,
				reserved,
				reserved,
				noApostrophe,
				notFeminine,
				notMasculine,
				singular,
				/* [9] */
				"zoomed",
				'pzum',
				'bool',
				"Is the window zoomed?",
				reserved,
				singleItem,
				notEnumerated,
				readWrite,
				enumsAreConstants,
				enumListCanRepeat,
				propertyIsValue,
				reserved,
				reserved,
				reserved,
				reserved,
				reserved,
				noApostrophe,
				notFeminine,
				notMasculine,
				singular,
				/* [10] */
				"name",
				'pnam',
				'itxt',
				"the title of the window",
				reserved,
				singleItem,
				notEnumerated,
				readWrite,
				enumsAreConstants,
				enumListCanRepeat,
				propertyIsValue,
				reserved,
				reserved,
				reserved,
				reserved,
				reserved,
				noApostrophe,
				notFeminine,
				notMasculine,
				singular,
				/* [11] */
				"visible",
				'pvis',
				'bool',
				"is the window visible?",
				reserved,
				singleItem,
				notEnumerated,
				readOnly,
				enumsAreConstants,
				enumListCanRepeat,
				propertyIsValue,
				reserved,
				reserved,
				reserved,
				reserved,
				reserved,
				noApostrophe,
				notFeminine,
				notMasculine,
				singular,
				/* [12] */
				"position",
				'ppos',
				'QDpt',
				"upper left coordinates of window",
				reserved,
				singleItem,
				notEnumerated,
				readOnly,
				enumsAreConstants,
				enumListCanRepeat,
				propertyIsValue,
				reserved,
				reserved,
				reserved,
				reserved,
				reserved,
				noApostrophe,
				notFeminine,
				notMasculine,
				singular
			},
			{	/* array Elements: 0 elements */
			},
			/* [3] */
			"document",
			'docu',
			"A Document",
			{	/* array Properties: 2 elements */
				/* [1] */
				"name",
				'pnam',
				'itxt',
				"the title of the document",
				reserved,
				singleItem,
				notEnumerated,
				readOnly,
				enumsAreConstants,
				enumListCanRepeat,
				propertyIsValue,
				reserved,
				reserved,
				reserved,
				reserved,
				reserved,
				noApostrophe,
				notFeminine,
				notMasculine,
				singular,
				/* [2] */
				"modified",
				'imod',
				'bool',
				"Has the document been modified since the"
				" last save?",
				reserved,
				singleItem,
				notEnumerated,
				readOnly,
				enumsAreConstants,
				enumListCanRepeat,
				propertyIsValue,
				reserved,
				reserved,
				reserved,
				reserved,
				reserved,
				noApostrophe,
				notFeminine,
				notMasculine,
				singular
			},
			{	/* array Elements: 0 elements */
			}
		},
		{	/* array ComparisonOps: 0 elements */
		},
		{	/* array Enumerations: 1 elements */
			/* [1] */
			'savo',
			{	/* array Enumerators: 3 elements */
				/* [1] */
				"yes",
				'yes ',
				"Save objects now",
				/* [2] */
				"no",
				'no  ',
				"Do not save objects",
				/* [3] */
				"ask",
				'ask ',
				"Ask the user whether to save"
			}
		},
		/* [3] */
		"Miscellaneous Standards",
		"Useful events that aren’t in any other s"
		"uite",
		'misc',
		0,
		0,
		{	/* array Events: 1 elements */
			/* [1] */
			"revert",
			"Revert an object to the most recently sa"
			"ved version",
			'misc',
			'rvrt',
			noReply,
			"",
			replyOptional,
			singleItem,
			notEnumerated,
			notTightBindingFunction,
			enumsAreConstants,
			enumListCanRepeat,
			replyIsValue,
			reserved,
			reserved,
			reserved,
			reserved,
			reserved,
			verbEvent,
			reserved,
			reserved,
			reserved,
			'obj ',
			"object to revert",
			directParamRequired,
			singleItem,
			notEnumerated,
			doesntChangeState,
			enumsAreConstants,
			enumListCanRepeat,
			directParamIsValue,
			directParamIsTarget,
			reserved,
			reserved,
			reserved,
			reserved,
			reserved,
			reserved,
			reserved,
			reserved,
			{	/* array OtherParams: 0 elements */
			}
		},
		{	/* array Classes: 0 elements */
		},
		{	/* array ComparisonOps: 0 elements */
		},
		{	/* array Enumerations: 0 elements */
		},
		/* [4] */
		"odds and ends",
		"Things that should be in some standard s"
		"uite, but aren’t",
		'Odds',
		1,
		1,
		{	/* array Events: 2 elements */
			/* [1] */
			"SetTellTarget",
			"Makes an object the “focus” of AppleEven"
			"ts",
			'ppnt',
			'sttg',
			noReply,
			"",
			replyRequired,
			singleItem,
			notEnumerated,
			notTightBindingFunction,
			enumsAreConstants,
			enumListCanRepeat,
			replyIsValue,
			reserved,
			reserved,
			reserved,
			reserved,
			reserved,
			verbEvent,
			reserved,
			reserved,
			reserved,
			noParams,
			"",
			directParamRequired,
			singleItem,
			notEnumerated,
			doesntChangeState,
			enumsAreConstants,
			enumListCanRepeat,
			directParamIsValue,
			directParamIsTarget,
			reserved,
			reserved,
			reserved,
			reserved,
			reserved,
			reserved,
			reserved,
			reserved,
			{	/* array OtherParams: 1 elements */
				/* [1] */
				"to",
				'data',
				'obj ',
				"reference to new focus of AppleEvents",
				optional,
				singleItem,
				notEnumerated,
				reserved,
				enumsAreConstants,
				enumListCanRepeat,
				paramIsValue,
				notParamIsTarget,
				reserved,
				reserved,
				reserved,
				reserved,
				prepositionParam,
				notFeminine,
				notMasculine,
				singular
			},
			/* [2] */
			"select",
			"Select the specified object",
			'misc',
			'slct',
			noReply,
			"",
			replyOptional,
			singleItem,
			notEnumerated,
			notTightBindingFunction,
			enumsAreConstants,
			enumListCanRepeat,
			replyIsValue,
			reserved,
			reserved,
			reserved,
			reserved,
			reserved,
			verbEvent,
			reserved,
			reserved,
			reserved,
			'obj ',
			"the object to select",
			directParamOptional,
			singleItem,
			notEnumerated,
			changesState,
			enumsAreConstants,
			enumListCanRepeat,
			directParamIsValue,
			directParamIsTarget,
			reserved,
			reserved,
			reserved,
			reserved,
			reserved,
			reserved,
			reserved,
			reserved,
			{	/* array OtherParams: 0 elements */
			}
		},
		{	/* array Classes: 0 elements */
		},
		{	/* array ComparisonOps: 0 elements */
		},
		{	/* array Enumerations: 0 elements */
		}
	}
};

resource 'aedt' (1000, "URL") {
	{	/* array: 1 elements */
		/* [1] */
		1196773964, 1196773964, 5000
	}
};

resource 'ALRT' (128, "About Box", purgeable) {
	{40, 20, 120, 410},
	128,
	{	/* array: 4 elements */
		/* [1] */
		OK, visible, silent,
		/* [2] */
		OK, visible, silent,
		/* [3] */
		OK, visible, silent,
		/* [4] */
		OK, visible, silent
	},
	alertPositionMainScreen
};

resource 'DITL' (128, "About Box", purgeable) {
	{	/* array DITLarray: 3 elements */
		/* [1] */
		{47, 319, 67, 377},
		Button {
			enabled,
			"OK"
		},
		/* [2] */
		{13, 23, 33, 390},
		StaticText {
			disabled,
			"PPEmbed - Gecko embedded in PowerPlant"
		},
		/* [3] */
		{33, 23, 53, 237},
		StaticText {
			disabled,
			"© 2000-2002 The Mozilla Organization."
		}
	}
};

resource 'ALRT' (204, "Low Memory Warning", locked, preload) {
	{40, 20, 142, 380},
	204,
	{	/* array: 4 elements */
		/* [1] */
		OK, visible, silent,
		/* [2] */
		OK, visible, silent,
		/* [3] */
		OK, visible, silent,
		/* [4] */
		OK, visible, silent
	},
	alertPositionMainScreen
};

resource 'DITL' (204, "Low Memory Warning", locked, preload) {
	{	/* array DITLarray: 2 elements */
		/* [1] */
		{69, 289, 89, 347},
		Button {
			enabled,
			"OK"
		},
		/* [2] */
		{10, 78, 56, 348},
		StaticText {
			disabled,
			"Memory is getting full. Please try to al"
			"leviate the problem by closing some docu"
			"ments. "
		}
	}
};

resource 'ALRT' (1500, "Confirm Profile Switch") {
	{40, 40, 142, 400},
	1500,
	{	/* array: 4 elements */
		/* [1] */
		OK, visible, sound1,
		/* [2] */
		OK, visible, sound1,
		/* [3] */
		OK, visible, sound1,
		/* [4] */
		OK, visible, sound1
	},
	alertPositionMainScreen
};

resource 'DITL' (1500, "Confirm Profile Switch") {
	{	/* array DITLarray: 3 elements */
		/* [1] */
		{68, 269, 88, 347},
		Button {
			enabled,
			"Continue"
		},
		/* [2] */
		{68, 195, 88, 259},
		Button {
			enabled,
			"Cancel"
		},
		/* [3] */
		{10, 78, 56, 348},
		StaticText {
			disabled,
			"Do you want to close all browser windows"
			" in order to change the profile?"
		}
	}
};

data 'ALRT' (1501, "Profile Logout") {
	$"0028 0028 008E 0190 05DD 5555"                      /* .(.(.é.ê.›UU */
};

resource 'DITL' (1501, "Profile Logout") {
	{	/* array DITLarray: 4 elements */
		/* [1] */
		{66, 244, 86, 342},
		Button {
			enabled,
			"Save Profile"
		},
		/* [2] */
		{66, 167, 86, 233},
		Button {
			enabled,
			"Cancel"
		},
		/* [3] */
		{66, 55, 86, 157},
		Button {
			enabled,
			"Clear Profile"
		},
		/* [4] */
		{10, 64, 45, 335},
		StaticText {
			disabled,
			"Do you want to close all windows and log"
			" out as \"^0?\""
		}
	}
};

resource 'STR#' (5000, "STRx_FileLocProviderStrings") {
	{	/* array StringArray: 9 elements */
		/* [1] */
		"Application Registry",
		/* [2] */
		"Profiles",
		/* [3] */
		"Defaults",
		/* [4] */
		"Pref",
		/* [5] */
		"Profile",
		/* [6] */
		"Res",
		/* [7] */
		"Chrome",
		/* [8] */
		"Plug-ins",
		/* [9] */
		"Search Plug-ins"
	}
};

resource 'STR#' (5001, "STRx_StdButtonTitles") {
	{	/* array StringArray: 10 elements */
		/* [1] */
		"",
		/* [2] */
		"OK",
		/* [3] */
		"Cancel",
		/* [4] */
		"Yes",
		/* [5] */
		"No",
		/* [6] */
		"Save",
		/* [7] */
		"Don't Save",
		/* [8] */
		"Revert",
		/* [9] */
		"Allow",
		/* [10] */
		"Deny All"
	}
};

resource 'STR#' (5002, "STRx_StdAlertStrings") {
	{	/* array StringArray: 4 elements */
		/* [1] */
		"Allow Opening of Popup Window",
		/* [2] */
		"This web page is attempting to open a po"
		"pup window. Choosing \"Deny All\" will pre"
		"vent this and all other popups from both"
		"ering you.",
		/* [3] */
		"Download in progress",
		/* [4] */
		"Choosing \"OK\" and closing the window wil"
		"l cancel the download."
	}
};

resource 'STR#' (5003, "STRx_DownloadStatus") {
	{	/* array StringArray: 8 elements */
		/* [1] */
		"@1 of @2 (at @3/sec)",
		/* [2] */
		"About 5 seconds",
		/* [3] */
		"About 10 seconds",
		/* [4] */
		"Less than a minute",
		/* [5] */
		"About a minute",
		/* [6] */
		"About @1 minutes",
		/* [7] */
		"About an hour",
		/* [8] */
		"About @1 hours"
	}
};
