/* ***** BEGIN LICENSE BLOCK *****
 ** Version: MPL 1.1/GPL 2.0/LGPL 2.1
 **
 ** The contents of this file are subject to the Mozilla Public License Version
 ** 1.1 (the "License"); you may not use this file except in compliance with
 ** the License. You may obtain a copy of the License at
 ** http://www.mozilla.org/MPL/
 **
 ** Software distributed under the License is distributed on an "AS IS" basis,
 ** WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 ** for the specific language governing rights and limitations under the
 ** License.
 **
 ** The Original Code is Mozilla Communicator client code.
 **
 ** The Initial Developer of the Original Code is
 ** Jungshik Shin <jshin@mailaps.org>.
 ** Portions created by the Initial Developer are Copyright (C) 2003
 ** the Initial Developer. All Rights Reserved.
 **
 ** Contributor(s):
 **
 ** Alternatively, the contents of this file may be used under the terms of
 ** either the GNU General Public License Version 2 or later (the "GPL"), or
 ** the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 ** in which case the provisions of the GPL or the LGPL are applicable instead
 ** of those above. If you wish to allow use of your version of this file only
 ** under the terms of either the GPL or the LGPL, and not to allow others to
 ** use your version of this file under the terms of the MPL, indicate your
 ** decision by deleting the provisions above and replace them with the notice
 ** and other provisions required by the GPL or the LGPL. If you do not delete
 ** the provisions above, a recipient may use your version of this file under
 ** the terms of any one of the MPL, the GPL or the LGPL.
 **
 ** ***** END LICENSE BLOCK ***** */

// Definitions to use in TSCII en/decoders.

#define TSC_KA    0xb8
#define TSC_NA    0xc9
// akara varisai
#define IS_TSC_CONSONANT1(c) ((c) >= TSC_KA && (c) <= TSC_NA) 
// grantha akara mey varisai
#define TSC_JA    0x83
#define TSC_HA    0x86
#define IS_TSC_CONSONANT2(c) ((c) >= TSC_JA && (c) <= TSC_HA) 

// union of two above
#define IS_TSC_CONSONANT(c)  (IS_TSC_CONSONANT1(c) || IS_TSC_CONSONANT2(c))

#define TSC_SA    0x85  
#define TSC_TA    0xbc  
#define TSC_RA    0xc3
#define TSC_KSSA  0x87 // Consonant Conjunct K.SSA : live 

#define TSC_KA_DEAD   0xec  // KA with virama/pulli
#define TSC_SA_DEAD   0x8a  // SA with virama/pulli
#define TSC_KSSA_DEAD 0x8c  // Consonant Conjunct K.SSA : dead

#define TSC_TI_LIGA   0xca  // Ligature of consonant TA and Vowel sign I
#define TSC_SRII_LIGA 0x82  // Ligature of SA + RA + II

// Two part vowel signs
#define UNI_VOWELSIGN_E 0x0BC6
#define TSC_VOWELSIGN_E 0xa6
#define TSC_LEFT_VOWELSIGN(u)      ((u) - UNI_VOWELSIGN_E + TSC_VOWELSIGN_E)
#define IS_UNI_LEFT_VOWELSIGN(u)   ((u) >= UNI_VOWELSIGN_E && (u) <= 0x0BC8)  
#define IS_UNI_2PARTS_VOWELSIGN(u) ((u) >= 0x0BCA && (u) <= 0x0BCC)  

#define UNI_TAMIL_START 0x0B80
#define IS_UNI_TAMIL(u) (UNI_TAMIL_START <= (u) && (u) < UNI_TAMIL_START + 0x80)

#define UNI_VIRAMA         0x0BCD
#define UNI_VOWELSIGN_I    0x0BBF
#define UNI_VOWELSIGN_II   0x0BC0
#define UNI_VOWELSIGN_U    0x0BC1
#define UNI_VOWELSIGN_UU   0x0BC2
#define UNI_SSA            0x0BB7
#define UNI_RA             0x0BB0

// Punctuation marks 
#define UNI_LSQ 0x2018
#define UNI_RSQ 0x2019
#define UNI_LDQ 0x201C
#define UNI_RDQ 0x201D
#define UNI_LEFT_SINGLE_QUOTE UNI_LSQ
#define UNI_RIGHT_SINGLE_QUOTE UNI_RSQ
#define UNI_LEFT_DOUBLE_QUOTE UNI_LDQ
#define UNI_RIGHT_DOUBLE_QUOTE UNI_RDQ
#define IS_UNI_SINGLE_QUOTE(u) (UNI_LSQ <= (u) && (u) <= UNI_RSQ)
#define IS_UNI_DOUBLE_QUOTE(u) (UNI_LDQ <= (u) && (u) <= UNI_RDQ)

#define TSC_LEFT_SINGLE_QUOTE 0x91
#define TSC_LEFT_DOUBLE_QUOTE 0x93

#define TSC_LEFT_VOWEL_PART(u) (((u) % 2) ? 0xa7 : 0xa6)
#define TSC_RIGHT_VOWEL_PART(u) (((u) != 0x0BCC) ? 0xa1 : 0xaa)
