/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "MPL"); you may not use this file
 * except in compliance with the MPL. You may obtain a copy of
 * the MPL at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the MPL is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the MPL for the specific language governing
 * rights and limitations under the MPL.
 * 
 * The Original Code is lineterm.
 * 
 * The Initial Developer of the Original Code is Ramalingam Saravanan.
 * Portions created by Ramalingam Saravanan <svn@xmlterm.org> are
 * Copyright (C) 1999 Ramalingam Saravanan. All Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License (the "GPL"), in which case
 * the provisions of the GPL are applicable instead of
 * those above. If you wish to allow use of your version of this
 * file only under the terms of the GPL and not to allow
 * others to use your version of this file under the MPL, indicate
 * your decision by deleting the provisions above and replace them
 * with the notice and other provisions required by the GPL.
 * If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

/* unistring.h: Unicode string operations header
 * (used by lineterm.h)
 * CPP options:
 *   USE_WCHAR:   use system wchar implementation, rather than unsigned short
 *   HAVE_WCSSTR: use wcsstr rather than wcswcs (used for wchar only)
 */

#ifndef _UNISTRING_H

#define _UNISTRING_H   1

#ifdef  __cplusplus
extern "C" {
#endif

/* Standard C header files */
#include <stdio.h>

/* Unicode character type:
 * Use either the wchar_t implementation or unsigned short
 */

#ifdef USE_WCHAR
#include <wchar.h>
typedef wchar_t UNICHAR;
#else /* !USE_WCHAR */
typedef unsigned short UNICHAR;
#endif

/* Unicode string functions:
 * use the wchar_t implementation for moment
 */

/** Encodes Unicode string US with NUS characters into UTF8 string S with
 * upto NS characters, returning the number of REMAINING Unicode characters
 * and the number of ENCODED Utf8 characters
 */
void ucstoutf8(const UNICHAR* us, int nus, char* s, int ns, 
               int* remaining, int* encoded);

/** Decodes UTF8 string S with NS characters to Unicode string US with
 * upto NUS characters, returning the number of REMAINING Utf8 characters
 * and the number of DECODED Unicode characters.
 * If skipNUL is non-zero, NUL input characters are skipped.
 * returns 0 if successful,
 *        -1 if an error occurred during decoding
 */
int utf8toucs(const char* s, int ns, UNICHAR* us, int nus,
              int skipNUL, int* remaining, int* decoded);

/** Prints Unicode string US with NUS characters to file stream STREAM,
 * escaping non-printable ASCII characters and all non-ASCII characters
 */
void ucsprint(FILE* stream, const UNICHAR* us, int nus);

/** Copy exactly n characters from plain character source string to UNICHAR
 * destination string, ignoring source characters past a null character and
 * padding the destination with null characters if necessary.
 */
UNICHAR* ucscopy(UNICHAR* dest, const char* srcplain, size_t n);

#ifdef USE_WCHAR

#define ucscpy    wcscpy
#define ucsncpy   wcsncpy

#define ucscat    wcscat
#define ucsncat   wcsncat

#define ucscmp    wcscmp
#define ucsncmp   wcsncmp

#define ucschr    wcschr
#define ucsrchr   wcsrchr

#define ucsspn    wcsspn
#define ucscspn   wcscspn

#define ucspbrk   wcspbrk

#ifdef HAVE_WCSSTR
#define ucsstr    wcsstr
#else
#define ucsstr    wcswcs
#endif

#define ucslen    wcslen

#define ucstok    wcstok

#else /* !USE_WCHAR */
/** Locates first occurrence of character within string and returns pointer
 * to it if found, else returning null pointer. (character may be NUL)
 */
UNICHAR* ucschr(const UNICHAR* str, const UNICHAR chr);

/** Locates last occurrence of character within string and returns pointer
 * to it if found, else returning null pointer. (character may be NUL)
 */
UNICHAR* ucsrchr(const UNICHAR* str, const UNICHAR chr);

/** Compare all characters between string1 and string2, returning
 * a zero value if all characters are equal, or returning
 * character1 - character2 for the first character that is different
 * between the two strings.
 * (Characters following a null character are not compared.)
 */
int ucscmp(register const UNICHAR* str1, register const UNICHAR* str2);

/** Compare upto n characters between string1 and string2, returning
 * a zero value if all compared characters are equal, or returning
 * character1 - character2 for the first character that is different
 * between the two strings.
 * (Characters following a null character are not compared.)
 */
int ucsncmp(const UNICHAR* str1, const UNICHAR* str2,
            size_t n);

/** Copy exactly n characters from source to destination, ignoring source
 * characters past a null character and padding the destination with null
 * characters if necessary.
 */
UNICHAR* ucsncpy(UNICHAR* dest, const UNICHAR* src,
                 size_t n);

/** Returns string length
 */
size_t ucslen(const UNICHAR* str);

/** Locates substring within string and returns pointer to it if found,
 * else returning null pointer. If substring has zero length, then full
 * string is returned.
 */
UNICHAR* ucsstr(const UNICHAR* str, const UNICHAR* substr);

/** Returns length of longest initial segment of string that contains
 * only the specified characters.
 */
size_t ucsspn(const UNICHAR* str, const UNICHAR* chars);
    
/** Returns length of longest initial segment of string that does not
 * contain any of the specified characters.
 */
size_t ucscspn(const UNICHAR* str, const UNICHAR* chars);

#endif  /* !USE_WCHAR */

/* unsigned short constants */
#define U_NUL         0x00U

#define U_CTL_A       0x01U
#define U_CTL_B       0x02U
#define U_CTL_C       0x03U
#define U_CTL_D       0x04U
#define U_CTL_E       0x05U
#define U_CTL_F       0x06U

#define U_BEL         0x07U      /* ^G */
#define U_BACKSPACE   0x08U      /* ^H */
#define U_TAB         0x09U      /* ^I */
#define U_LINEFEED    0x0AU      /* ^J */

#define U_CTL_K       0x0BU
#define U_CTL_L       0x0CU

#define U_CRETURN     0x0DU      /* ^M */

#define U_CTL_N       0x0EU
#define U_CTL_O       0x0FU
#define U_CTL_P       0x10U
#define U_CTL_Q       0x11U
#define U_CTL_R       0x12U
#define U_CTL_S       0x13U
#define U_CTL_T       0x14U
#define U_CTL_U       0x15U
#define U_CTL_V       0x16U
#define U_CTL_W       0x17U
#define U_CTL_X       0x18U
#define U_CTL_Y       0x19U
#define U_CTL_Z       0x1AU

#define U_ESCAPE      0x1BU      /* escape */

#define U_SPACE       0x20U      /* space */
#define U_EXCLAMATION 0x21U      /* ! */
#define U_DBLQUOTE    0x22U      /* " */
#define U_NUMBER      0x23U      /* # */
#define U_DOLLAR      0x24U      /* $ */
#define U_PERCENT     0x25U      /* % */
#define U_AMPERSAND   0x26U      /* & */
#define U_SNGLQUOTE   0x27U      /* ' */
#define U_LPAREN      0x28U      /* ( */
#define U_RPAREN      0x29U      /* ) */

#define U_STAR        0x2AU      /* * */
#define U_PLUS        0x2BU      /* + */
#define U_COMMA       0x2CU      /* , */
#define U_DASH        0x2DU      /* - */
#define U_PERIOD      0x2EU      /* . */
#define U_SLASH       0x2FU      /* / */

#define U_ZERO        0x30U      /* 0 */
#define U_ONE         0x31U      /* 1 */
#define U_TWO         0x32U      /* 2 */
#define U_THREE       0x33U      /* 3 */
#define U_FOUR        0x34U      /* 4 */
#define U_FIVE        0x35U      /* 5 */
#define U_SIX         0x36U      /* 6 */
#define U_SEVEN       0x37U      /* 7 */
#define U_EIGHT       0x38U      /* 8 */
#define U_NINE        0x39U      /* 9 */

#define U_COLON       0x3AU      /* : */
#define U_SEMICOLON   0x3BU      /* ; */
#define U_LESSTHAN    0x3CU      /* < */
#define U_EQUALS      0x3DU      /* = */
#define U_GREATERTHAN 0x3EU      /* > */
#define U_QUERYMARK   0x3FU      /* ? */

#define U_ATSIGN      0x40U      /* @ */

#define U_A_CHAR      0x41U      /* A */
#define U_B_CHAR      0x42U      /* B */
#define U_C_CHAR      0x43U      /* C */
#define U_D_CHAR      0x44U      /* D */
#define U_E_CHAR      0x45U      /* E */
#define U_F_CHAR      0x46U      /* F */
#define U_G_CHAR      0x47U      /* G */
#define U_H_CHAR      0x48U      /* H */
#define U_I_CHAR      0x49U      /* I */
#define U_J_CHAR      0x4AU      /* J */
#define U_K_CHAR      0x4BU      /* K */
#define U_L_CHAR      0x4CU      /* L */
#define U_M_CHAR      0x4DU      /* M */
#define U_N_CHAR      0x4EU      /* N */
#define U_O_CHAR      0x4FU      /* O */
#define U_P_CHAR      0x50U      /* P */
#define U_Q_CHAR      0x51U      /* Q */
#define U_R_CHAR      0x52U      /* R */
#define U_S_CHAR      0x53U      /* S */
#define U_T_CHAR      0x54U      /* T */
#define U_U_CHAR      0x55U      /* U */
#define U_V_CHAR      0x56U      /* V */
#define U_W_CHAR      0x57U      /* W */
#define U_X_CHAR      0x58U      /* X */
#define U_Y_CHAR      0x59U      /* Y */
#define U_Z_CHAR      0x5AU      /* Z */

#define U_LBRACKET    0x5BU      /* [ */
#define U_BACKSLASH   0x5CU      /* \ */
#define U_RBRACKET    0x5DU      /* ] */

#define U_CARET       0x5EU      /* ^ */
#define U_UNDERSCORE  0x5FU      /* _ */
#define U_BACKQUOTE   0x60U      /* ` */

#define U_a_CHAR      0x61U      /* a */
#define U_b_CHAR      0x62U      /* b */
#define U_c_CHAR      0x63U      /* c */
#define U_d_CHAR      0x64U      /* d */
#define U_e_CHAR      0x65U      /* e */
#define U_f_CHAR      0x66U      /* f */
#define U_g_CHAR      0x67U      /* g */
#define U_h_CHAR      0x68U      /* h */
#define U_i_CHAR      0x69U      /* i */
#define U_j_CHAR      0x6AU      /* j */
#define U_k_CHAR      0x6BU      /* k */
#define U_l_CHAR      0x6CU      /* l */
#define U_m_CHAR      0x6DU      /* m */
#define U_n_CHAR      0x6EU      /* n */
#define U_o_CHAR      0x6FU      /* o */
#define U_p_CHAR      0x70U      /* p */
#define U_q_CHAR      0x71U      /* q */
#define U_r_CHAR      0x72U      /* r */
#define U_s_CHAR      0x73U      /* s */
#define U_t_CHAR      0x74U      /* t */
#define U_u_CHAR      0x75U      /* u */
#define U_v_CHAR      0x76U      /* v */
#define U_w_CHAR      0x77U      /* w */
#define U_x_CHAR      0x78U      /* x */
#define U_y_CHAR      0x79U      /* y */
#define U_z_CHAR      0x7AU      /* z */

#define U_LCURLY      0x7BU      /* { */
#define U_VERTBAR     0x7CU      /* | */
#define U_RCURLY      0x7DU      /* } */

#define U_TILDE       0x7EU      /* ~ */
#define U_DEL         0x7FU      /* delete */

#define U_LATIN1LO    0xA0U      /* lowest Latin1 extension character */
#define U_NOBRKSPACE  0xA0U      /* no-break space */
#define U_LATIN1HI    0xFFU      /* highest Latin1 extension character */

#define U_PRIVATE0    0xE000U    /* first private use Unicode character */

#define IS_ASCII_LETTER(x) ( (((x) >= (UNICHAR)U_A_CHAR) &&  \
                              ((x) <= (UNICHAR)U_Z_CHAR)) || \
                             (((x) >= (UNICHAR)U_a_CHAR) &&  \
                              ((x) <= (UNICHAR)U_z_CHAR)) )

#define IS_ASCII_DIGIT(x)  ( ((x) >= (UNICHAR)U_ZERO) && \
                             ((x) <= (UNICHAR)U_NINE) )


#ifdef  __cplusplus
}
#endif

#endif /* _UNISTRING_H */
