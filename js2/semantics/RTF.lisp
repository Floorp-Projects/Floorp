;;; The contents of this file are subject to the Mozilla Public
;;; License Version 1.1 (the "License"); you may not use this file
;;; except in compliance with the License. You may obtain a copy of
;;; the License at http://www.mozilla.org/MPL/
;;; 
;;; Software distributed under the License is distributed on an "AS
;;; IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
;;; implied. See the License for the specific language governing
;;; rights and limitations under the License.
;;; 
;;; The Original Code is the Language Design and Prototyping Environment.
;;; 
;;; The Initial Developer of the Original Code is Netscape Communications
;;; Corporation.  Portions created by Netscape Communications Corporation are
;;; Copyright (C) 1999 Netscape Communications Corporation.  All
;;; Rights Reserved.
;;; 
;;; Contributor(s):   Waldemar Horwat <waldemar@acm.org>

;;;
;;; RTF reader and writer
;;;
;;; Waldemar Horwat (waldemar@acm.org)
;;;


(defvar *rtf-author* "Waldemar Horwat")
(defvar *rtf-company* "Netscape")


;;; 1440 twips/inch
;;;   20 twips/pt

(defparameter *rtf-definitions*
  '((:rtf-intro rtf 1 mac ansicpg 10000 uc 1 deff 0 deflang 2057 deflangfe 2057)
    
    ;Fonts
    ((+ :rtf-intro) :fonttbl)
    (:fonttbl (fonttbl :fonts))
    
    (:times f 0)
    ((+ :fonts) (:times froman fcharset 256 fprq 2 (* panose "02020603050405020304") "Times New Roman;"))
    (:symbol f 3)
    ((+ :fonts) (:symbol ftech fcharset 2 fprq 2 "Symbol;"))
    (:helvetica f 4)
    ((+ :fonts) (:helvetica fnil fcharset 256 fprq 2 "Helvetica;"))
    (:courier f 5)
    ((+ :fonts) (:courier fmodern fcharset 256 fprq 2 "Courier New;"))
    (:palatino f 6)
    ((+ :fonts) (:palatino fnil fcharset 256 fprq 2 "Palatino;"))
    (:zapf-chancery f 7)
    ((+ :fonts) (:zapf-chancery fscript fcharset 256 fprq 2 "Zapf Chancery;"))
    (:zapf-dingbats f 8)
    ((+ :fonts) (:zapf-dingbats ftech fcharset 2 fprq 2 "Zapf Dingbats;"))
    
    
    ;Color table
    ((+ :rtf-intro) :colortbl)
    (:colortbl (colortbl ";"                                ;0
                         red 0 green 0 blue 0 ";"           ;1
                         red 0 green 0 blue 255 ";"         ;2
                         red 0 green 255 blue 255 ";"       ;3
                         red 0 green 255 blue 0 ";"         ;4
                         red 255 green 0 blue 255 ";"       ;5
                         red 255 green 0 blue 0 ";"         ;6
                         red 255 green 255 blue 0 ";"       ;7
                         red 255 green 255 blue 255 ";"     ;8
                         red 0 green 0 blue 128 ";"         ;9
                         red 0 green 128 blue 128 ";"       ;10
                         red 0 green 128 blue 0 ";"         ;11
                         red 128 green 0 blue 128 ";"       ;12
                         red 128 green 0 blue 0 ";"         ;13
                         red 128 green 128 blue 0 ";"       ;14
                         red 128 green 128 blue 128 ";"     ;15
                         red 192 green 192 blue 192 ";"     ;16
                         red 0 green 64 blue 0 ";"          ;17
                         red #x33 green #x66 blue #x00 ";"));18
    (:black cf 1)
    (:blue cf 2)
    (:aqua cf 3)
    (:lime cf 4)
    (:fuchsia cf 5)
    (:red cf 6)
    (:yellow cf 7)
    (:white cf 8)
    (:navy cf 9)
    (:teal cf 10)
    (:green cf 11)
    (:purple cf 12)
    (:maroon cf 13)
    (:olive cf 14)
    (:gray cf 15)
    (:silver cf 16)
    (:dark-green cf 17)
    (:color336600 cf 18)
    
    
    ;Misc.
    (:space " ")
    (:tab2 tab)
    (:tab3 tab)
    (:nbhy _)             ;Non-breaking hyphen
    (:8-pt fs 16)
    (:9-pt fs 18)
    (:10-pt fs 20)
    (:12-pt fs 24)
    (:14-pt fs 28)
    (:no-language lang 1024)
    (:english-us lang 1033)
    (:english-uk lang 2057)
    
    (:english :english-uk)
    
    (:reset-section sectd)
    (:new-section sect)
    (:reset-paragraph pard plain)
    ((:new-paragraph t) par)
    ((:new-line t) line)
    
    ;Symbols (-10 suffix means 10-point, etc.)
    ((:bullet 1) bullet)
    ((:minus 1) endash)
    ((:not-equal 1) u 8800 \' 173)
    ((:less-or-equal 1) u 8804 \' 178)
    ((:greater-or-equal 1) u 8805 \' 179)
    ((:infinity 1) u 8734 \' 176)
    ((:left-single-quote 1) lquote)
    ((:right-single-quote 1) rquote)
    ((:apostrophe 1) rquote)
    ((:left-double-quote 1) ldblquote)
    ((:right-double-quote 1) rdblquote)
    ((:left-angle-quote 1) u 171 \' 199)
    ((:right-angle-quote 1) u 187 \' 200)
    ((:for-all-10 1) (field (* fldinst "SYMBOL 34 \\f \"Symbol\" \\s 10") (fldrslt :symbol :10-pt)))
    ((:exists-10 1) (field (* fldinst "SYMBOL 36 \\f \"Symbol\" \\s 10") (fldrslt :symbol :10-pt)))
    ((:bottom-10 1) (field (* fldinst "SYMBOL 94 \\f \"Symbol\" \\s 10") (fldrslt :symbol :10-pt)))
    ((:assign-10 2) (field (* fldinst "SYMBOL 172 \\f \"Symbol\" \\s 10") (fldrslt :symbol :10-pt)))
    ((:up-arrow-10 1) (field (* fldinst "SYMBOL 173 \\f \"Symbol\" \\s 10") (fldrslt :symbol :10-pt)))
    ((:function-arrow-10 2) (field (* fldinst "SYMBOL 174 \\f \"Symbol\" \\s 10") (fldrslt :symbol :10-pt)))
    ((:cartesian-product-10 2) (field (* fldinst "SYMBOL 180 \\f \"Symbol\" \\s 10") (fldrslt :symbol :10-pt)))
    ((:identical-10 2) (field (* fldinst "SYMBOL 186 \\f \"Symbol\" \\s 10") (fldrslt :symbol :10-pt)))
    ((:circle-plus-10 2) (field (* fldinst "SYMBOL 197 \\f \"Symbol\" \\s 10") (fldrslt :symbol :10-pt)))
    ((:empty-10 2) (field (* fldinst "SYMBOL 198 \\f \"Symbol\" \\s 10") (fldrslt :symbol :10-pt)))
    ((:intersection-10 1) (field (* fldinst "SYMBOL 199 \\f \"Symbol\" \\s 10") (fldrslt :symbol :10-pt)))
    ((:union-10 1) (field (* fldinst "SYMBOL 200 \\f \"Symbol\" \\s 10") (fldrslt :symbol :10-pt)))
    ((:subset-10 2) (field (* fldinst "SYMBOL 204 \\f \"Symbol\" \\s 10") (fldrslt :symbol :10-pt)))
    ((:subset-eq-10 2) (field (* fldinst "SYMBOL 205 \\f \"Symbol\" \\s 10") (fldrslt :symbol :10-pt)))
    ((:member-10 2) (field (* fldinst "SYMBOL 206 \\f \"Symbol\" \\s 10") (fldrslt :symbol :10-pt)))
    ((:not-member-10 2) (field (* fldinst "SYMBOL 207 \\f \"Symbol\" \\s 10") (fldrslt :symbol :10-pt)))
    ((:label-assign-10 2) (field (* fldinst "SYMBOL 220 \\f \"Symbol\" \\s 10") (fldrslt :symbol :10-pt)))
    ((:derives-10 2) (field (* fldinst "SYMBOL 222 \\f \"Symbol\" \\s 10") (fldrslt :symbol :10-pt)))
    ((:left-triangle-bracket-10 1) (field (* fldinst "SYMBOL 225 \\f \"Symbol\" \\s 10") (fldrslt :symbol :10-pt)))
    ((:left-ceiling-10 1) (field (* fldinst "SYMBOL 233 \\f \"Symbol\" \\s 10") (fldrslt :symbol :10-pt)))
    ((:left-floor-10 1) (field (* fldinst "SYMBOL 235 \\f \"Symbol\" \\s 10") (fldrslt :symbol :10-pt)))
    ((:right-triangle-bracket-10 1) (field (* fldinst "SYMBOL 241 \\f \"Symbol\" \\s 10") (fldrslt :symbol :10-pt)))
    ((:right-ceiling-10 1) (field (* fldinst "SYMBOL 249 \\f \"Symbol\" \\s 10") (fldrslt :symbol :10-pt)))
    ((:right-floor-10 1) (field (* fldinst "SYMBOL 251 \\f \"Symbol\" \\s 10") (fldrslt :symbol :10-pt)))
    ((:big-plus-10 2) (field (* fldinst "SYMBOL 58 \\f \"Zapf Dingbats\" \\s 10") (fldrslt :zapf-dingbats :10-pt)))
    
    ((:alpha 1) (field (* fldinst "SYMBOL 97 \\f \"Symbol\" \\s 10") (fldrslt :symbol :10-pt)))
    ((:beta 1) (field (* fldinst "SYMBOL 98 \\f \"Symbol\" \\s 10") (fldrslt :symbol :10-pt)))
    ((:chi 1) (field (* fldinst "SYMBOL 99 \\f \"Symbol\" \\s 10") (fldrslt :symbol :10-pt)))
    ((:delta 1) (field (* fldinst "SYMBOL 100 \\f \"Symbol\" \\s 10") (fldrslt :symbol :10-pt)))
    ((:epsilon 1) (field (* fldinst "SYMBOL 101 \\f \"Symbol\" \\s 10") (fldrslt :symbol :10-pt)))
    ((:phi 1) (field (* fldinst "SYMBOL 102 \\f \"Symbol\" \\s 10") (fldrslt :symbol :10-pt)))
    ((:gamma 1) (field (* fldinst "SYMBOL 103 \\f \"Symbol\" \\s 10") (fldrslt :symbol :10-pt)))
    ((:eta 1) (field (* fldinst "SYMBOL 104 \\f \"Symbol\" \\s 10") (fldrslt :symbol :10-pt)))
    ((:iota 1) (field (* fldinst "SYMBOL 105 \\f \"Symbol\" \\s 10") (fldrslt :symbol :10-pt)))
    ((:kappa 1) (field (* fldinst "SYMBOL 107 \\f \"Symbol\" \\s 10") (fldrslt :symbol :10-pt)))
    ((:lambda 1) (field (* fldinst "SYMBOL 108 \\f \"Symbol\" \\s 10") (fldrslt :symbol :10-pt)))
    ((:mu 1) (field (* fldinst "SYMBOL 109 \\f \"Symbol\" \\s 10") (fldrslt :symbol :10-pt)))
    ((:nu 1) (field (* fldinst "SYMBOL 110 \\f \"Symbol\" \\s 10") (fldrslt :symbol :10-pt)))
    ((:omicron 1) (field (* fldinst "SYMBOL 111 \\f \"Symbol\" \\s 10") (fldrslt :symbol :10-pt)))
    ((:pi 1) (field (* fldinst "SYMBOL 112 \\f \"Symbol\" \\s 10") (fldrslt :symbol :10-pt)))
    ((:theta 1) (field (* fldinst "SYMBOL 113 \\f \"Symbol\" \\s 10") (fldrslt :symbol :10-pt)))
    ((:rho 1) (field (* fldinst "SYMBOL 114 \\f \"Symbol\" \\s 10") (fldrslt :symbol :10-pt)))
    ((:sigma 1) (field (* fldinst "SYMBOL 115 \\f \"Symbol\" \\s 10") (fldrslt :symbol :10-pt)))
    ((:tau 1) (field (* fldinst "SYMBOL 116 \\f \"Symbol\" \\s 10") (fldrslt :symbol :10-pt)))
    ((:upsilon 1) (field (* fldinst "SYMBOL 117 \\f \"Symbol\" \\s 10") (fldrslt :symbol :10-pt)))
    ((:omega 1) (field (* fldinst "SYMBOL 119 \\f \"Symbol\" \\s 10") (fldrslt :symbol :10-pt)))
    ((:xi 1) (field (* fldinst "SYMBOL 120 \\f \"Symbol\" \\s 10") (fldrslt :symbol :10-pt)))
    ((:psi 1) (field (* fldinst "SYMBOL 121 \\f \"Symbol\" \\s 10") (fldrslt :symbol :10-pt)))
    ((:zeta 1) (field (* fldinst "SYMBOL 122 \\f \"Symbol\" \\s 10") (fldrslt :symbol :10-pt)))

    ((:capital-omega 1) (field (* fldinst "SYMBOL 87 \\f \"Symbol\" \\s 10") (fldrslt :symbol :10-pt)))
    
    
    ;Styles
    ((+ :rtf-intro) :stylesheet)
    (:stylesheet (stylesheet :styles))
    (:asian-keywords aspalpha aspnum faauto)
    
    (:normal-num 0)
    (:normal s :normal-num)
    ((+ :styles) (widctlpar :asian-keywords :10-pt :english snext :normal-num "Normal;"))
    
    (:body-text-num 1)
    (:body-text s :body-text-num qj sa 180 widctlpar :asian-keywords :10-pt :english)
    ((+ :styles) (:body-text sbasedon :normal-num snext :body-text-num "Body Text;"))
    
    (:body-text-narrow-num 2)
    (:body-text-narrow s :body-text-narrow-num qj sa 40 widctlpar :asian-keywords :10-pt :english)
    ((+ :styles) (:body-text-narrow sbasedon :body-text-num snext :body-text-narrow-num "Body Text Narrow;"))
    (:semantic-comment :body-text-narrow)
    
    (:header-num 5)
    (:header s :header-num nowidctlpar :asian-keywords tqr tx 10080 :10-pt :english)
    ((+ :styles) (:header sbasedon :normal-num snext :header-num "header;"))
    
    (:footer-num 6)
    (:footer s :footer-num nowidctlpar :asian-keywords tqc tx 5040 :10-pt :english)
    ((+ :styles) (:footer sbasedon :normal-num snext :footer-num "footer;"))
    
    (:grammar-num 8)
    (:grammar s :grammar-num nowidctlpar :asian-keywords hyphpar 0 :10-pt :english)
    ((+ :styles) (:grammar sbasedon :normal-num snext :grammar-num "Grammar;"))
    
    (:grammar-header-num 9)
    (:grammar-header s :grammar-header-num sb 60 sa 180 keep keepn nowidctlpar :asian-keywords hyphpar 0 b :10-pt :english)
    ((+ :styles) (:grammar-header sbasedon :normal-num snext :grammar-lhs-num "Grammar Header;"))
    
    (:grammar-lhs-num 10)
    (:grammar-lhs s :grammar-lhs-num fi -540 li 720 keep keepn nowidctlpar :asian-keywords hyphpar 0 :10-pt :english)
    ((+ :styles) (:grammar-lhs sbasedon :grammar-num snext :grammar-rhs-num "Grammar LHS;"))
    
    (:grammar-lhs-last-num 11)
    (:grammar-lhs-last s :grammar-lhs-last-num fi -540 li 720 sa 180 keep nowidctlpar :asian-keywords hyphpar 0 :10-pt :english)
    ((+ :styles) (:grammar-lhs-last sbasedon :grammar-lhs-num snext :grammar-lhs-num "Grammar LHS Last;"))
    
    (:grammar-rhs-num 12)
    (:grammar-rhs s :grammar-rhs-num fi -360 li 720 keep keepn nowidctlpar :asian-keywords tx 540 hyphpar 0 :10-pt :english)
    ((+ :styles) (:grammar-rhs sbasedon :grammar-num snext :grammar-rhs-num "Grammar RHS;"))
    
    (:grammar-rhs-last-num 13)
    (:grammar-rhs-last s :grammar-rhs-last-num fi -360 li 720 sa 180 keep nowidctlpar :asian-keywords tx 540 hyphpar 0 :10-pt :english)
    ((+ :styles) (:grammar-rhs-last sbasedon :grammar-rhs-num snext :grammar-lhs-num "Grammar RHS Last;"))
    
    (:grammar-argument-num 14)
    (:grammar-argument s :grammar-argument-num fi -540 li 720 sa 180 keep nowidctlpar :asian-keywords hyphpar 0 :10-pt :english)
    ((+ :styles) (:grammar-argument sbasedon :grammar-num snext :grammar-lhs-num "Grammar Argument;"))
    
    (:algorithm-simple-num 16)
    (:algorithm-simple s :algorithm-simple-num li 480 sa 180 keep widctlpar :asian-keywords hyphpar 0 :10-pt :english)
    ((+ :styles) (:algorithm-simple sbasedon :body-text-num snext :algorithm-simple-num "Algorithm Simple;"))
    
    (:algorithm-0-num 17)
    (:algorithm-0 s :algorithm-0-num fi -480 li 720 keep keepn nowidctlpar :asian-keywords hyphpar 0 :10-pt :english)
    ((+ :styles) (:algorithm-0 sbasedon :normal-num snext :algorithm-0-num "Algorithm 0;"))
    
    (:algorithm-0-last-num 18)
    (:algorithm-0-last s :algorithm-0-last-num fi -480 li 720 sa 180 keep nowidctlpar :asian-keywords hyphpar 0 :10-pt :english)
    ((+ :styles) (:algorithm-0-last sbasedon :algorithm-0-num snext :body-text-num "Algorithm 0 Last;"))
    (:algorithm-stmt :algorithm-0-last)
    
    (:algorithm-1-num 19)
    (:algorithm-1 s :algorithm-1-num fi -480 li 960 keep keepn nowidctlpar :asian-keywords hyphpar 0 :10-pt :english)
    ((+ :styles) (:algorithm-1 sbasedon :algorithm-0-num snext :algorithm-1-num "Algorithm 1;"))
    
    (:algorithm-1-last-num 20)
    (:algorithm-1-last s :algorithm-1-last-num fi -480 li 960 sa 180 keep nowidctlpar :asian-keywords hyphpar 0 :10-pt :english)
    ((+ :styles) (:algorithm-1-last sbasedon :algorithm-1-num snext :body-text-num "Algorithm 1 Last;"))
    
    (:algorithm-2-num 21)
    (:algorithm-2 s :algorithm-2-num fi -480 li 1200 keep keepn nowidctlpar :asian-keywords hyphpar 0 :10-pt :english)
    ((+ :styles) (:algorithm-2 sbasedon :algorithm-1-num snext :algorithm-2-num "Algorithm 2;"))
    
    (:algorithm-2-last-num 22)
    (:algorithm-2-last s :algorithm-2-last-num fi -480 li 1200 sa 180 keep nowidctlpar :asian-keywords hyphpar 0 :10-pt :english)
    ((+ :styles) (:algorithm-2-last sbasedon :algorithm-2-num snext :body-text-num "Algorithm 2 Last;"))
    
    (:algorithm-3-num 23)
    (:algorithm-3 s :algorithm-3-num fi -480 li 1440 keep keepn nowidctlpar :asian-keywords hyphpar 0 :10-pt :english)
    ((+ :styles) (:algorithm-3 sbasedon :algorithm-2-num snext :algorithm-3-num "Algorithm 3;"))
    
    (:algorithm-4-num 24)
    (:algorithm-4 s :algorithm-4-num fi -480 li 1680 keep keepn nowidctlpar :asian-keywords hyphpar 0 :10-pt :english)
    ((+ :styles) (:algorithm-4 sbasedon :algorithm-3-num snext :algorithm-4-num "Algorithm 4;"))
    
    (:algorithm-5-num 25)
    (:algorithm-5 s :algorithm-5-num fi -480 li 1920 keep keepn nowidctlpar :asian-keywords hyphpar 0 :10-pt :english)
    ((+ :styles) (:algorithm-5 sbasedon :algorithm-4-num snext :algorithm-5-num "Algorithm 5;"))
    
    (:algorithm-6-num 26)
    (:algorithm-6 s :algorithm-6-num fi -480 li 2160 keep keepn nowidctlpar :asian-keywords hyphpar 0 :10-pt :english)
    ((+ :styles) (:algorithm-6 sbasedon :algorithm-5-num snext :algorithm-6-num "Algorithm 6;"))
    
    (:algorithm-7-num 27)
    (:algorithm-7 s :algorithm-7-num fi -480 li 2400 keep keepn nowidctlpar :asian-keywords hyphpar 0 :10-pt :english)
    ((+ :styles) (:algorithm-7 sbasedon :algorithm-6-num snext :algorithm-7-num "Algorithm 7;"))
    
    (:algorithm-8-num 28)
    (:algorithm-8 s :algorithm-8-num fi -480 li 2640 keep keepn nowidctlpar :asian-keywords hyphpar 0 :10-pt :english)
    ((+ :styles) (:algorithm-8 sbasedon :algorithm-7-num snext :algorithm-8-num "Algorithm 8;"))
    
    (:algorithm-9-num 29)
    (:algorithm-9 s :algorithm-9-num fi -480 li 2880 keep keepn nowidctlpar :asian-keywords hyphpar 0 :10-pt :english)
    ((+ :styles) (:algorithm-9 sbasedon :algorithm-8-num snext :algorithm-9-num "Algorithm 9;"))
    
    (:algorithm-0-wide-num 30)
    (:algorithm-0-wide s :algorithm-0-wide-num fi -480 li 720 sa 40 keep nowidctlpar :asian-keywords hyphpar 0 :10-pt :english)
    ((+ :styles) (:algorithm-0-wide sbasedon :algorithm-0-num snext :algorithm-0-num "Algorithm 0 Wide;"))
    
    (:algorithm-1-wide-num 31)
    (:algorithm-1-wide s :algorithm-1-wide-num fi -480 li 960 sa 40 keep nowidctlpar :asian-keywords hyphpar 0 :10-pt :english)
    ((+ :styles) (:algorithm-1-wide sbasedon :algorithm-1-num snext :algorithm-1-num "Algorithm 1 Wide;"))
    
    (:algorithm-2-wide-num 32)
    (:algorithm-2-wide s :algorithm-2-wide-num fi -480 li 1200 sa 40 keep nowidctlpar :asian-keywords hyphpar 0 :10-pt :english)
    ((+ :styles) (:algorithm-2-wide sbasedon :algorithm-2-num snext :algorithm-2-num "Algorithm 2 Wide;"))
    
    (:algorithm-3-wide-num 33)
    (:algorithm-3-wide s :algorithm-3-wide-num fi -480 li 1440 sa 40 keep nowidctlpar :asian-keywords hyphpar 0 :10-pt :english)
    ((+ :styles) (:algorithm-3-wide sbasedon :algorithm-3-num snext :algorithm-3-num "Algorithm 3 Wide;"))
    
    (:algorithm-4-wide-num 34)
    (:algorithm-4-wide s :algorithm-4-wide-num fi -480 li 1680 sa 40 keep nowidctlpar :asian-keywords hyphpar 0 :10-pt :english)
    ((+ :styles) (:algorithm-4-wide sbasedon :algorithm-4-num snext :algorithm-4-num "Algorithm 4 Wide;"))
    
    (:default-paragraph-font-num 40)
    (:default-paragraph-font cs :default-paragraph-font-num)
    ((+ :styles) (* :default-paragraph-font additive "Default Paragraph Font;"))
    
    (:page-number-num 41)
    (:page-number cs :page-number-num)
    ((+ :styles) (* :page-number additive sbasedon :default-paragraph-font-num "page number;"))
    
    (:character-literal-num 42)
    (:character-literal cs :character-literal-num :courier :navy :no-language)
    ((+ :styles) (* :character-literal additive sbasedon :default-paragraph-font-num "Character Literal;"))
    
    (:character-literal-control-num 43)
    (:character-literal-control cs :character-literal-control-num :times :navy :no-language)
    ((+ :styles) (* :character-literal-control additive sbasedon :default-paragraph-font-num "Character Literal Control;"))
    
    (:terminal-num 44)
    (:terminal cs :terminal-num b :palatino :teal :no-language)
    ((+ :styles) (* :terminal additive sbasedon :default-paragraph-font-num "Terminal;"))
    
    (:terminal-keyword-num 45)
    (:terminal-keyword cs :terminal-keyword-num b :courier :blue :no-language)
    ((+ :styles) (* :terminal-keyword additive sbasedon :terminal-num "Terminal Keyword;"))
    
    (:nonterminal-num 46)
    (:nonterminal cs :nonterminal-num i :palatino :maroon :no-language)
    ((+ :styles) (* :nonterminal additive sbasedon :default-paragraph-font-num "Nonterminal;"))
    
    (:nonterminal-attribute-num 47)
    (:nonterminal-attribute cs :nonterminal-attribute-num i 0 :no-language)
    ((+ :styles) (* :nonterminal-attribute additive sbasedon :default-paragraph-font-num "Nonterminal Attribute;"))
    
    (:nonterminal-argument-num 48)
    (:nonterminal-argument cs :nonterminal-argument-num :no-language)
    ((+ :styles) (* :nonterminal-argument additive sbasedon :default-paragraph-font-num "Nonterminal Argument;"))
    
    (:semantic-keyword-num 50)
    (:semantic-keyword cs :semantic-keyword-num b :times)
    ((+ :styles) (* :semantic-keyword additive sbasedon :default-paragraph-font-num "Semantic Keyword;"))
    
    (:type-name-num 51)
    (:type-name cs :type-name-num scaps :times :red :no-language)
    ((+ :styles) (* :type-name additive sbasedon :default-paragraph-font-num "Type Name;"))
    
    (:field-name-num 52)
    (:field-name cs :field-name-num :helvetica :no-language)
    ((+ :styles) (* :field-name additive sbasedon :default-paragraph-font-num "Field Name;"))
    
    (:tag-name-num 53)
    (:tag-name cs :tag-name-num :helvetica b :no-language)
    ((+ :styles) (* :tag-name additive sbasedon :default-paragraph-font-num "Tag Name;"))
    
    (:global-variable-num 54)
    (:global-variable cs :global-variable-num i :times :dark-green :no-language)
    ((+ :styles) (* :global-variable additive sbasedon :default-paragraph-font-num "Global Variable;"))
    
    (:variable-num 55)
    (:variable cs :variable-num i :times :green :no-language)
    ((+ :styles) (* :variable additive sbasedon :default-paragraph-font-num "Variable;"))
    
    (:action-name-num 56)
    (:action-name cs :action-name-num :zapf-chancery :purple :no-language)
    ((+ :styles) (* :action-name additive sbasedon :default-paragraph-font-num "Action Name;"))
    
    
    (:heading1-num 61)
    (:heading1 s :heading1-num qj fi -720 li 720 sb 240 sa 180 keep keepn widctlpar :asian-keywords hyphpar 0 level 1 b :14-pt :english)
    ((+ :styles) (:heading1 sbasedon :normal-num snext :body-text-num "heading 1;"))
    (:section-heading :heading1)
    
    (:heading2-num 62)
    (:heading2 s :heading2-num qj fi -720 li 720 sb 120 sa 180 keep keepn widctlpar :asian-keywords hyphpar 0 level 2 b :12-pt :english)
    ((+ :styles) (:heading2 sbasedon :heading1-num snext :body-text-num "heading 2;"))
    (:subsection-heading :heading2)
    
    (:heading3-num 63)
    (:heading3 s :heading3-num qj fi -720 li 720 sb 60 sa 120 keep keepn widctlpar :asian-keywords hyphpar 0 level 3 b :10-pt :english)
    ((+ :styles) (:heading3 sbasedon :heading2-num snext :body-text-num "heading 3;"))

    (:heading4-num 64)
    (:heading4 s :heading4-num qj fi -720 li 720 sa 120 keep keepn widctlpar :asian-keywords hyphpar 0 level 4 b :10-pt :english)
    ((+ :styles) (:heading4 sbasedon :heading3-num snext :body-text-num "heading 4;"))


    (:sample-code-num 70)
    (:sample-code s :sample-code-num li 1440 sa 180 keep nowidctlpar :asian-keywords hyphpar 0 b :courier :blue :10-pt :no-language)
    ((+ :styles) (:sample-code sbasedon :normal-num snext :body-text-num "Sample Code;"))
    
    
    ;Headers and Footers
    (:page-number-field (field (* fldinst (:page-number " PAGE ")) (fldrslt (:page-number :no-language "1"))))
    (:left-header-group headerl :reset-paragraph :header)
    (:right-header-group headerr :reset-paragraph :header)
    
    ;(:left-footer-group (footerl :reset-paragraph :footer tab :page-number-field))
    ;(:right-footer-group (footerr :reset-paragraph :footer tab :page-number-field))
    
    
    ;Document Formatting
    (:docfmt margl 720
             margr 720
             margb 720
             gutter 720
             facingp        ;mirror left/right gutters
             widowctrl
             ftnbj          ;footnotes at bottom of page
             aenddoc        ;endnotes at end of document
             noxlattoyen    ;don't translate backslash to yen
             hyphcaps 0     ;don't hyphenate capitalized words
             fet 0          ;footnotes only -- no endnotes
             formshade      ;shade form fields
             viewkind 4     ;normal view mode
             viewscale 125  ;125% view
             pgbrdrhead     ;page border surrounds header
             pgbrdrfoot)    ;page border surrounds footer
    
    
    ;Section Formatting
    
    
    ;Specials
    (:text :english)
    (:invisible v)
    ((:but-not 6) (b "except"))
    ((:begin-negative-lookahead 13) "[lookahead" :not-member-10 "{")
    ((:end-negative-lookahead 2) "}]")
    ((:line-break 12) "[line" ~ "break]")
    ((:no-line-break 15) "[no" ~ "line" ~ "break]")
    (:subscript sub)
    (:plain-subscript b 0 i 0 :subscript)
    (:superscript super)
    ((:action-begin 1) "[")
    ((:action-end 1) "]")
    ((:vector-begin 1) (b "["))
    ((:vector-end 1) (b "]"))
    ((:empty-vector 2) (b "[]"))
    ((:vector-construct 1) (b "|"))
    ((:vector-append 2) :circle-plus-10)
    ((:tuple-begin 1) (b :left-triangle-bracket-10))
    ((:tuple-end 1) (b :right-triangle-bracket-10))
    ((:record-begin 1) (b :left-triangle-bracket-10 :left-triangle-bracket-10))
    ((:record-end 1) (b :right-triangle-bracket-10 :right-triangle-bracket-10))
    ((:true 4) (:global-variable "true"))
    ((:false 5) (:global-variable "false"))
    ((:unique 6) (:semantic-keyword "unique"))

    (:mod-date s :normal-num qr sa 120 widctlpar :asian-keywords :10-pt :english i)
    ))


(defparameter *division-widths*
  '((:nowrap . 0)
    (:level . 4)
    (:level-wide . 4)
    (:algorithm . 0)
    (:grammar-rule . 0)
    (:js2 . 0)
    (:es4 . 0)))


(defparameter *division-style-specializations*
  '((:nowrap . t)
    (:level (:statement . :statement-1)
            (:statement-1 . :statement-2)
            (:statement-2 . :statement-3)
            (:statement-3 . :statement-4)
            (:statement-4 . :statement-5)
            (:statement-5 . :statement-6)
            (:statement-6 . :statement-7)
            (:statement-7 . :statement-8)
            (:statement-8 . :statement-9)
            (:statement-last . :statement-1-last)
            (:statement-1-last . :statement-2-last)
            (:statement-2-last . :statement-3-last)
            (:statement-3-last . :statement-4-last))
    (:level-wide (:statement . :statement-1)
                 (:statement-1 . :statement-2)
                 (:statement-2 . :statement-3)
                 (:statement-3 . :statement-4)
                 (:statement-4 . :statement-5)
                 (:statement-5 . :statement-6)
                 (:statement-6 . :statement-7)
                 (:statement-7 . :statement-8)
                 (:statement-8 . :statement-9)
                 (:statement-last . :statement-1-wide)
                 (:statement-1-last . :statement-2-wide)
                 (:statement-2-last . :statement-3-wide)
                 (:statement-3-last . :statement-4-wide))
    (:algorithm (:statement . :algorithm-0)
                (:statement-1 . :algorithm-1)
                (:statement-2 . :algorithm-2)
                (:statement-3 . :algorithm-3)
                (:statement-4 . :algorithm-4)
                (:statement-5 . :algorithm-5)
                (:statement-6 . :algorithm-6)
                (:statement-7 . :algorithm-7)
                (:statement-8 . :algorithm-8)
                (:statement-9 . :algorithm-9)
                (:statement-last . :algorithm-0-last)
                (:statement-1-last . :algorithm-1-last)
                (:statement-2-last . :algorithm-2-last)
                (:statement-1-wide . :algorithm-1-wide)
                (:statement-2-wide . :algorithm-2-wide)
                (:statement-3-wide . :algorithm-3-wide)
                (:statement-4-wide . :algorithm-4-wide))
    (:grammar-rule . t)
    (:js2 . t)
    (:es4 . t)))


;;; ------------------------------------------------------------------------------------------------------
;;; SIMPLE LINE BREAKER

(defparameter *limited-line-right-margin* 100)

; Housekeeping dynamic variables
(defvar *current-limited-lines*)  ;Items written so far via break-line to the innermost write-limited-lines
(defvar *current-limited-lines-non-empty*)  ;True if something was written to *current-limited-lines*
(defvar *current-limited-position*)   ;Number of characters written since the last newline to *current-limited-lines*


; Capture the text written by the emitter function to its single parameter
; (an output stream), dividing the text as specified by dynamically scoped calls
; to break-line.  Return the text as a base-string.
(defun write-limited-lines (emitter)
  (let ((limited-stream (make-string-output-stream :element-type #-mcl 'character #+mcl 'base-character))
        (*current-limited-lines* (make-string-output-stream :element-type #-mcl 'character #+mcl 'base-character))
        (*current-limited-lines-non-empty* nil)
        (*current-limited-position* 0))
    (funcall emitter limited-stream)
    (break-line limited-stream)
    (get-output-stream-string *current-limited-lines*)))


; Capture the text written by the emitter body to stream-var,
; dividing the text as specified by dynamically scoped calls
; to break-line.  Write the result to the stream-var stream.
(defmacro write-limited-block (stream-var &body emitter)
  `(progn
     (write-string
      (write-limited-lines #'(lambda (,stream-var) ,@emitter))
      ,stream-var)
     nil))


; Indicate that this is a potential place for a line break in the stream provided
; by write-limited-lines.  If subdivide is true, also indicate that line breaks can
; be inserted anywhere between the previous such point indicated by break-line
; (or the beginning of write-limited-lines if this is the first call to break-line)
; and this point.
(defun break-line (limited-stream &optional subdivide)
  (let* ((new-chars (get-output-stream-string limited-stream))
         (length (length new-chars)))
    (unless (zerop length)
      (labels
        ((subdivide-new-chars (start)
           (let ((length-remaining (- length start))
                 (room-on-line (- *limited-line-right-margin* *current-limited-position*)))
             (if (>= room-on-line length-remaining)
               (progn
                 (write-string new-chars *current-limited-lines* :start start)
                 (incf *current-limited-position* length-remaining))
               (let ((end (+ start room-on-line)))
                 (write-string new-chars *current-limited-lines* :start start :end end)
                 (write-char #\newline *current-limited-lines*)
                 (setq *current-limited-position* 0)
                 (subdivide-new-chars end))))))
        
        (let ((position (+ *current-limited-position* length))
              (has-newlines (find #\newline new-chars)))
          (cond
           ((or has-newlines
                (and (> position *limited-line-right-margin*) (not subdivide)))
            (when *current-limited-lines-non-empty*
              (write-char #\newline *current-limited-lines*))
            (write-string new-chars *current-limited-lines*)
            ;Force a line break if break-line is called again and the current
            ;new-chars contained a line break.
            (setq *current-limited-position*
                  (if has-newlines
                    (1+ *limited-line-right-margin*)
                    length)))
           ((<= position *limited-line-right-margin*)
            (write-string new-chars *current-limited-lines*)
            (setq *current-limited-position* position))
           ((>= *current-limited-position* *limited-line-right-margin*)
            (write-char #\newline *current-limited-lines*)
            (setq *current-limited-position* 0)
            (subdivide-new-chars 0))
           (t (subdivide-new-chars 0)))
          (setq *current-limited-lines-non-empty* t))))))


;;; ------------------------------------------------------------------------------------------------------
;;; RTF READER


; Return true if char can be a part of an RTF control word.
(defun rtf-control-word-char? (char)
  (or (and (char>= char #\a) (char<= char #\z))
      (and (char>= char #\A) (char<= char #\Z))))


; Intern str, reversing the case of its characters.  If str had any upper-case characters,
; set the 'rtf-control-word property of the new symbol to the original string.
(defun intern-rtf-control-word (str)
  (if (some #'(lambda (char) (char<= char #\Z)) str)
    (let* ((new-str (map 'string #'(lambda (char) (code-char (logxor (char-code char) 32))) str))
           (symbol (intern new-str)))
      (setf (get symbol 'rtf-control-word) str)
      symbol)
    (intern (string-upcase str))))


; Read RTF from the character stream and return it in list form.
; Each { ... } group is a sublist.
; Each RTF control symbol or word is represented by a lisp symbol.
; If an RTF control has a numeric argument, then its lisp symbol is followed
; by an integer equal to the argument's value.
; Newlines not escaped by backslashes are ignored.
(defun read-rtf (stream)
  (labels
    ((read (&optional (eof-error-p t))
       (read-char stream eof-error-p nil))
     
     (read-group (nested contents-reverse)
       (let ((char (read nested)))
         (case char
           ((nil) (nreverse contents-reverse))
           (#\} (if nested
                  (nreverse contents-reverse)
                  (error "Mismatched }")))
           (#\{ (read-group nested (cons (read-group t nil) contents-reverse)))
           (#\\ (read-group nested (nreconc (read-control) contents-reverse)))
           (#\newline (read-group nested contents-reverse))
           (t (read-text nested (list char) contents-reverse)))))
     
     (read-text (nested chars-reverse contents-reverse)
       (let ((char (read nested)))
         (case char
           ((nil)
            (cons (coerce (nreverse chars-reverse) 'string) (nreverse contents-reverse)))
           ((#\{ #\} #\\)
            (let ((s (coerce (nreverse chars-reverse) 'string)))
              (unread-char char stream)
              (read-group nested (cons s contents-reverse))))
           (#\newline (read-text nested chars-reverse contents-reverse))
           (t (read-text nested (cons char chars-reverse) contents-reverse)))))
     
     (read-integer (value need-digit)
       (let* ((char (read))
              (digit (digit-char-p char)))
         (cond
          (digit (read-integer (+ (* value 10) digit) nil))
          (need-digit (error "Empty number"))
          ((eql char #\space) value)
          (t (unread-char char stream)
             value))))
     
     (read-hex (n-digits)
       (let ((value 0))
         (dotimes (n n-digits)
           (let ((digit (digit-char-p (read) 16)))
             (unless digit
               (error "Bad hex digit"))
             (setq value (+ (* value 16) digit))))
         value))
     
     ; The result list may be destructively modified.
     (read-control ()
       (let ((char (read)))
         (if (rtf-control-word-char? char)
           (let* ((control-string (read-control-word (list char)))
                  (control-symbol (intern-rtf-control-word control-string))
                  (char (read)))
             (case char
               (#\space (list control-symbol))
               (#\- (list control-symbol (- (read-integer 0 t))))
               ((#\0 #\1 #\2 #\3 #\4 #\5 #\6 #\7 #\8 #\9)
                (unread-char char stream)
                (list control-symbol (read-integer 0 t)))
               (t (unread-char char stream)
                  (list control-symbol))))
           (let* ((control-string (string char))
                  (control-symbol (intern control-string)))
             (if (eq control-symbol '\')
               (list control-symbol (read-hex 2))
               (list control-symbol))))))
     
     (read-control-word (chars-reverse)
       (let ((char (read)))
         (if (rtf-control-word-char? char)
           (read-control-word (cons char chars-reverse))
           (progn
             (unread-char char stream)
             (coerce (nreverse chars-reverse) 'string))))))
    
    (read-group nil nil)))


; Read RTF from the text file with the given name (relative to the
; local directory) and return it in list form.
(defun read-rtf-from-local-file (filename)
  (with-open-file (stream (filename-to-semantic-engine-pathname filename)
                          :direction :input)
    (read-rtf stream)))


;;; ------------------------------------------------------------------------------------------------------
;;; RTF WRITER


(defconstant *rtf-special* '(#\\ #\{ #\}))


; Return the string with characters in *rtf-special* preceded by backslashes.
; If there are no such characters, the returned string may be eq to the input string.
(defun escape-rtf (string)
  (let ((i (position-if #'(lambda (char) (member char *rtf-special*)) string)))
    (if i
      (let* ((string-length (length string))
             (result-string (make-array string-length :element-type #-mcl 'character #+mcl 'base-character :adjustable t :fill-pointer i)))
        (replace result-string string)
        (do ((i i (1+ i)))
            ((= i string-length))
          (let ((char (char string i)))
            (when (member char *rtf-special*)
              (vector-push-extend #\\ result-string))
            (vector-push-extend char result-string)))
        result-string)
      string)))


; Write the string with characters in *rtf-special* preceded by backslashes and not allowing
; linebreaks between a backslash and the character it is escaping.  Start with the character
; at offset start in the given string.
(defun write-escaped-rtf (stream string start)
  (let ((end (position-if #'(lambda (char) (member char *rtf-special*)) string :start start)))
    (if end
      (progn
        (unless (= start end)
          (write-string string stream :start start :end end)
          (break-line stream t))
        (write-char #\\ stream)
        (write-char (char string end) stream)
        (break-line stream)
        (write-escaped-rtf stream string (1+ end)))
      (unless (= start (length string))
        (write-string string stream :start start)
        (break-line stream t)))))


; Write RTF to the character stream.  See read-rtf for a description
; of the layout of the rtf list.
(defun write-rtf (rtf &optional (stream t))
  (labels
    ((write-group-contents (rtf stream)
       (let ((first-rtf (first rtf))
             (rest-rtf (rest rtf)))
         (cond
          ((listp first-rtf)
           (write-group first-rtf stream t))
          ((stringp first-rtf)
           (write-escaped-rtf stream first-rtf 0))
          ((symbolp first-rtf)
           (write-char #\\ stream)
           (write (get first-rtf 'rtf-control-word first-rtf) :stream stream)
           (cond
            ((alpha-char-p (char (symbol-name first-rtf) 0))
             (when (integerp (first rest-rtf))
               (write (first rest-rtf) :stream stream)
               (setq rest-rtf (rest rest-rtf)))
             (let ((first-rest (first rest-rtf)))
               (when (and (stringp first-rest)
                          (or (zerop (length first-rest))
                              (let ((ch (char first-rest 0)))
                                (or (alphanumericp ch)
                                    (eql ch #\space)
                                    (eql ch #\-)
                                    (eql ch #\+)))))
                 (write-char #\space stream))))
            ((eq first-rtf '\')
             (unless (integerp (first rest-rtf))
               (error "Bad rtf: ~S" rtf))
             (format stream "~2,'0x" (first rest-rtf))
             (setq rest-rtf (rest rest-rtf)))))
          (t (error "Bad rtf: ~S" rtf)))
         (when rest-rtf
           (break-line stream)
           (write-group-contents rest-rtf stream))))
     
     (write-group (rtf stream nested)
       (write-limited-block stream
         (when nested
           (write-char #\{ stream))
         (when rtf
           (write-group-contents rtf stream))
         (when nested
           (write-char #\} stream)))))
    
    (with-standard-io-syntax
      (let ((*print-readably* nil)
            (*print-escape* nil)
            (*print-case* :downcase))
        (write-group rtf stream nil)))))


; Write RTF to the text file with the given name (relative to the
; local directory).
(defun write-rtf-to-local-file (filename rtf)
  (with-open-file (stream (filename-to-semantic-engine-pathname filename)
                          :direction :output
                          :if-exists :supersede
                          #+mcl :external-format #+mcl "RTF "
                          #+mcl :mac-file-creator #+mcl "MSWD")
    (write-rtf rtf stream)))


;;; ------------------------------------------------------------------------------------------------------
;;; RTF STREAMS

(defstruct (rtf-stream (:include markup-stream)
                       (:constructor allocate-rtf-stream (env head tail level logical-line-width logical-position enclosing-styles))
                       (:copier nil)
                       (:predicate rtf-stream?))
  (enclosing-styles nil :type list)  ;A list of enclosing division styles
  (style nil :type symbol))          ;Current section or paragraph style or nil if none or emitting paragraph contents


(defmethod print-object ((rtf-stream rtf-stream) stream)
  (print-unreadable-object (rtf-stream stream :identity t)
    (write-string "rtf-stream" stream)))


; Make a new, empty, open rtf-stream with the given definitions for its markup-env.
(defun make-rtf-stream (markup-env level logical-line-width logical-position enclosing-styles)
  (let ((head (list nil)))
    (allocate-rtf-stream markup-env head head level logical-line-width logical-position enclosing-styles)))


; Make a new, empty, open, top-level rtf-stream with the given definitions
; for its markup-env.
(defun make-top-level-rtf-stream (rtf-definitions)
  (let ((head (list nil))
        (markup-env (make-markup-env nil)))
    (markup-env-define-alist markup-env rtf-definitions)
    (allocate-rtf-stream markup-env head head *markup-stream-top-level* *markup-logical-line-width* nil nil)))


; Append a block to the end of the rtf-stream.  The block may be inlined
; if nothing else follows it in the rtf-stream.
(defun rtf-stream-append-or-inline-block (rtf-stream block)
  (assert-type block list)
  (when block
    (let ((pretail (markup-stream-tail rtf-stream)))
      (markup-stream-append1 rtf-stream block)
      (setf (markup-stream-pretail rtf-stream) pretail))))


; Return the approximate width of the rtf item; return t if it is a line break.
; Also allow rtf groups as long as they do not contain line breaks.
(defmethod markup-group-width ((rtf-stream rtf-stream) item)
  (if (consp item)
    (reduce #'+ item :key #'(lambda (subitem) (markup-group-width rtf-stream subitem)))
    (markup-width rtf-stream item)))


; Return the information group or nil if none is needed.
; Any of the inputs can be nil, in which case the corresponding info entry is omitted.
(defun generate-document-info (title author company time)
  (and (or title author company time)
       (cons 'info
             (nconc
              (and title (list (list 'title (assert-type title string))))
              (and author (list (list 'author (assert-type author string))
                                (list 'operator author)))
              (and time (multiple-value-bind (second minute hour day month year) (decode-universal-time time)
                          (let ((rtf-time (list 'yr year 'mo month 'dy day 'hr hour 'min minute 'sec second)))
                            (list (cons 'creatim rtf-time)
                                  (cons 'revtim rtf-time)
                                  (list 'edmins 0)))))
              (and company (list (list '* 'company (assert-type company string))))))))


; Return the header group.
(defun generate-left-header-group (title time)
  (list :left-header-group :page-number-field 'tab (assert-type title string) " " (time-to-short-string time)))

(defun generate-right-header-group (title time)
  (list :right-header-group (assert-type title string) " " (time-to-short-string time) 'tab :page-number-field))


; Create a top-level rtf-stream and call emitter to emit its contents.
; emitter takes one argument -- an rtf-stream to which it should emit paragraphs.
; Return the top-level rtf-stream.
(defun depict-rtf-top-level (title emitter &optional (rtf-definitions *rtf-definitions*))
  (let* ((top-rtf-stream (make-top-level-rtf-stream rtf-definitions))
         (rtf-stream (make-rtf-stream (markup-stream-env top-rtf-stream)
                                      *markup-stream-paragraph-level*
                                      *markup-logical-line-width*
                                      nil
                                      nil))
         (time (get-universal-time)))
    (markup-stream-append1 rtf-stream :rtf-intro)
    (let ((info (generate-document-info title *rtf-author* *rtf-company* time)))
      (when info
        (markup-stream-append1 rtf-stream info)))
    (markup-stream-append1 rtf-stream :docfmt)
    (markup-stream-append1 rtf-stream :reset-section)
    (markup-stream-append1 rtf-stream (generate-left-header-group title time))
    (markup-stream-append1 rtf-stream (generate-right-header-group title time))
    ;(markup-stream-append1 rtf-stream :left-footer-group)
    ;(markup-stream-append1 rtf-stream :right-footer-group)
    (funcall emitter rtf-stream)
    (markup-stream-append1 top-rtf-stream (markup-stream-unexpanded-output rtf-stream))
    top-rtf-stream))


; Create a top-level rtf-stream and call emitter to emit its contents.
; emitter takes one argument -- an rtf-stream to which it should emit paragraphs.
; Write the resulting RTF to the text file with the given name (relative to the
; local directory).
(defun depict-rtf-to-local-file (filename title emitter &optional (rtf-definitions *rtf-definitions*))
  (let ((top-rtf-stream (depict-rtf-top-level title emitter rtf-definitions)))
    (write-rtf-to-local-file filename (markup-stream-output top-rtf-stream)))
  filename)


(defun debug-depict-rtf (title emitter &optional (rtf-definitions *rtf-definitions*))
  (let ((top-rtf-stream (depict-rtf-top-level title emitter rtf-definitions)))
    (markup-stream-output top-rtf-stream)))


; Return the markup accumulated in the markup-stream after expanding all of its macros.
; The markup-stream is closed after this function is called.
(defmethod markup-stream-output ((rtf-stream rtf-stream))
  (markup-env-expand (markup-stream-env rtf-stream) (markup-stream-unexpanded-output rtf-stream) nil))


; Return a freshly consed list of rtf-items that represent the characters in the string except that
; spaces are replaced by nonbreakable spaces.
(defun convert-rtf-string-spaces-to-nbsps (string)
  (let ((rtf-items nil))
    (labels
      ((escape-remainder (start)
         (let ((i (position #\space string :start start)))
           (if i
             (progn
               (unless (= i start)
                 (push (subseq string start i) rtf-items))
               (push '~ rtf-items)
               (escape-remainder (1+ i)))
             (push (if (zerop start) string (subseq string start)) rtf-items)))))
      (escape-remainder 0)
      (nreverse rtf-items))))


; Destructively convert spaces inside strings in rtf-item into nonbreakable spaces.
; Return a freshly consed list of the results.
(defun convert-rtf-item-spaces-to-nbsps (rtf-item)
  (cond
   ((stringp rtf-item)
    (convert-rtf-string-spaces-to-nbsps rtf-item))
   ((or (characterp rtf-item) (symbolp rtf-item) (integerp rtf-item))
    (list rtf-item))
   ((consp rtf-item)
    (list (convert-rtf-list-spaces-to-nbsps rtf-item)))
   (t (error "Bad rtf-item: ~S" rtf-item))))


; Destructively convert spaces inside strings in rtf-list into nonbreakable spaces.
(defun convert-rtf-list-spaces-to-nbsps (rtf-list)
  (mapcan #'convert-rtf-item-spaces-to-nbsps rtf-list))


(defun depict-nowrap-rtf-style (rtf-stream emitter)
  (let ((saved-tail (rtf-stream-tail rtf-stream)))
    (setf (rtf-stream-pretail rtf-stream) nil)
    (prog1
      (funcall emitter rtf-stream)
      (setf (cdr saved-tail) (convert-rtf-list-spaces-to-nbsps (cdr saved-tail)))
      (setf (rtf-stream-tail rtf-stream) (last saved-tail))
      (setf (rtf-stream-pretail rtf-stream) nil))))


(defmethod depict-division-style-f ((rtf-stream rtf-stream) division-style flatten emitter)
  (assert-true (= (markup-stream-level rtf-stream) *markup-stream-paragraph-level*))
  (assert-true (symbolp division-style))
  (if (or (null division-style)
          (and flatten (member division-style (rtf-stream-enclosing-styles rtf-stream))))
    (funcall emitter rtf-stream)
    (let ((width (cdr (assert-non-null (assoc division-style *division-widths*)))))
      (decf (rtf-stream-logical-line-width rtf-stream) width)
      (push division-style (rtf-stream-enclosing-styles rtf-stream))
      (prog1
        (if (eq division-style :nowrap)
          (depict-nowrap-rtf-style rtf-stream emitter)
          (funcall emitter rtf-stream))
        (pop (rtf-stream-enclosing-styles rtf-stream))
        (incf (rtf-stream-logical-line-width rtf-stream) width)))))


(defun specialize-paragraph-style (rtf-stream paragraph-style)
  (dolist (enclosing-style (rtf-stream-enclosing-styles rtf-stream))
    (let ((map (assoc enclosing-style *division-style-specializations*)))
      (unless map
        (error "Cannot specialize division style ~S" enclosing-style))
      (setq map (cdr map))
      (unless (eq map t)
        (let ((new-style (assoc paragraph-style map)))
          (unless new-style
            (error "Cannot specialize division style ~S containing a ~S paragraph" enclosing-style paragraph-style))
          (setq paragraph-style (cdr new-style))))))
  paragraph-style)


; rtf is the output from a rtf-stream consisting of paragraphs.
; Return rtf flattened to a single paragraph with the given paragraph-style with spaces inserted
; between the component paragraphs.  saved-style is the style inherited at the beginning of the rtf.
; May destroy the original rtf list.
(defun flatten-rtf-paragraphs (rtf paragraph-style saved-style)
  (assert-true rtf)
  (let* ((delete-next nil)
         (flattened-rtf (mapcan #'(lambda (item)
                                    (cond
                                     (delete-next
                                      (setq delete-next nil)
                                      nil)
                                     ((eq item :new-paragraph) (list " "))
                                     ((eq item :reset-paragraph)
                                      (setq delete-next t)
                                      nil)
                                     (t (list item))))
                                rtf)))
    (assert-true (not delete-next))
    (unless (eq paragraph-style saved-style)
      (setq flattened-rtf (list* :reset-paragraph paragraph-style flattened-rtf)))
    (let ((last (last flattened-rtf)))
      (assert-true (string= (car last) " "))
      (setf (car last) :new-paragraph))
    flattened-rtf))


(defmethod depict-division-block-f ((rtf-stream rtf-stream) paragraph-style paragraph-styles division-styles emitter)
  (declare (ignore paragraph-styles division-styles))
  (assert-true (= (markup-stream-level rtf-stream) *markup-stream-paragraph-level*))
  (assert-true (and paragraph-style (symbolp paragraph-style)))
  (let ((paragraph-style (specialize-paragraph-style rtf-stream paragraph-style))
        (logical-line-width (rtf-stream-logical-line-width rtf-stream))
        (saved-division-length (rtf-stream-division-length rtf-stream))
        (saved-style (rtf-stream-style rtf-stream))
        (saved-tail (rtf-stream-tail rtf-stream)))
    (setf (rtf-stream-division-length rtf-stream) 0)
    (setf (rtf-stream-pretail rtf-stream) nil)
    (prog1
      (funcall emitter rtf-stream)
      (let ((inner-length (rtf-stream-division-length rtf-stream)))
        (unless (eq inner-length t)
          (if (> inner-length logical-line-width)
            (setq inner-length t)
            (let ((flattened (flatten-rtf-paragraphs (cdr saved-tail) paragraph-style saved-style)))
              (setf (cdr saved-tail) flattened)
              (setf (rtf-stream-tail rtf-stream) (last saved-tail))
              (setf (rtf-stream-pretail rtf-stream) nil))))
        (setf (rtf-stream-division-length rtf-stream) saved-division-length)
        (increment-division-length rtf-stream inner-length)))))


(defmethod depict-paragraph-f ((rtf-stream rtf-stream) paragraph-style emitter)
  (assert-true (= (markup-stream-level rtf-stream) *markup-stream-paragraph-level*))
  (assert-true (and paragraph-style (symbolp paragraph-style)))
  (let ((paragraph-style (specialize-paragraph-style rtf-stream paragraph-style)))
    (assert-true (and paragraph-style (symbolp paragraph-style)))
    (unless (eq paragraph-style (rtf-stream-style rtf-stream))
      (markup-stream-append1 rtf-stream :reset-paragraph)
      (markup-stream-append1 rtf-stream paragraph-style)
      (setf (rtf-stream-style rtf-stream) paragraph-style))
    (let ((logical-position (make-logical-position)))
      (setf (rtf-stream-level rtf-stream) *markup-stream-content-level*)
      (setf (rtf-stream-logical-position rtf-stream) logical-position)
      (prog1
        (funcall emitter rtf-stream)
        (assert-true (and (eq logical-position (rtf-stream-logical-position rtf-stream))
                          (null (logical-position-n-soft-breaks logical-position))))
        (increment-division-length rtf-stream (if (= (logical-position-n-hard-breaks logical-position) 0)
                                                (1+ (logical-position-position logical-position))
                                                t))
        (setf (rtf-stream-level rtf-stream) *markup-stream-paragraph-level*)
        (setf (rtf-stream-logical-position rtf-stream) nil)
        (markup-stream-append1 rtf-stream :new-paragraph)))))


(defmethod depict-char-style-f ((rtf-stream rtf-stream) char-style emitter)
  (assert-true (>= (markup-stream-level rtf-stream) *markup-stream-content-level*))
  (if char-style
    (let ((inner-rtf-stream (make-rtf-stream (rtf-stream-env rtf-stream)
                                             *markup-stream-content-level*
                                             (rtf-stream-logical-line-width rtf-stream)
                                             (rtf-stream-logical-position rtf-stream)
                                             (rtf-stream-enclosing-styles rtf-stream))))
      (assert-true (symbolp char-style))
      (markup-stream-append1 inner-rtf-stream char-style)
      (prog1
        (funcall emitter inner-rtf-stream)
        (rtf-stream-append-or-inline-block rtf-stream (markup-stream-unexpanded-output inner-rtf-stream))))
    (funcall emitter rtf-stream)))


(defmethod ensure-no-enclosing-style ((rtf-stream rtf-stream) style)
  (declare (ignore style)))


(defmethod save-division-style ((rtf-stream rtf-stream))
  nil)


(defmethod with-saved-division-style-f ((rtf-stream rtf-stream) saved-division-style flatten emitter)
  (declare (ignore saved-division-style flatten))
  (assert-true (= (markup-stream-level rtf-stream) *markup-stream-paragraph-level*))
  (funcall emitter rtf-stream))


(defmethod depict-anchor ((rtf-stream rtf-stream) link-prefix link-name duplicate)
  (declare (ignore link-prefix link-name duplicate))
  (assert-true (= (markup-stream-level rtf-stream) *markup-stream-content-level*)))


(defmethod depict-link-reference-f ((rtf-stream rtf-stream) link-prefix link-name external emitter)
  (declare (ignore link-prefix link-name external))
  (assert-true (= (markup-stream-level rtf-stream) *markup-stream-content-level*))
  (funcall emitter rtf-stream))


;;; ------------------------------------------------------------------------------------------------------
;;; RTF EDITING


(defun each-rtf-tag (rtf tag f)
  (unless (endp rtf)
    (let ((first (first rtf)))
      (cond
       ((eq first tag) (funcall f rtf))
       ((consp first) (each-rtf-tag first tag f))))
    (each-rtf-tag (rest rtf) tag f)))


(defun find-rtf-tag (rtf tag)
  (let ((n-found 0)
        (found nil))
    (each-rtf-tag rtf tag #'(lambda (rtf)
                              (incf n-found)
                              (setq found rtf)))
    (unless (= n-found 1)
      (error "Rtf tag ~S isn't unique" tag))
    found))


(defun delete-unused-rtf-data (rtf label table-label)
  (let ((histogram (make-hash-table)))
    (each-rtf-tag rtf label #'(lambda (rtf)
                                (let ((n (second rtf)))
                                  (unless (integerp n)
                                    (error "Bad ~S: ~S" label n))
                                  (incf (gethash n histogram 0)))))
    (let ((sorted-ns (sort (hash-table-keys histogram) #'<))
          (new-n 0))
      (dolist (n sorted-ns)
        (if (> (gethash n histogram) 1)
          (progn
            (setf (gethash n histogram) (incf new-n))
            (format *terminal-io* "~S -> ~S~%" n new-n))
          (setf (gethash n histogram) t))))
    (let ((table (find-rtf-tag rtf table-label)))
      (setf (cdr table)
            (delete-if #'(lambda (listoverride)
                           (let* ((n (second (find-rtf-tag listoverride label)))
                                  (action (gethash n histogram)))
                             (assert-true action)
                             (eq action t)))
                       (cdr table))))
    (each-rtf-tag rtf label #'(lambda (rtf)
                                (let* ((n (second rtf))
                                       (action (gethash n histogram)))
                                  (assert-true (integerp action))
                                  (setf (second rtf) action))))))


(defun delete-datafields (rtf)
  (each-rtf-tag
   rtf 'fldinst
   #'(lambda (rtf)
       (setf (cdr rtf)
             (delete-if
              #'(lambda (r)
                  (and (consp r)
                       (progn
                         (when (eq (first r) 'fs)
                           (setq r (cddr r)))
                         (let ((df (first r)))
                           (and (consp df) (eq (first df) '*) (eq (second df) 'datafield))))))
              (cdr rtf)))))
  (each-rtf-tag rtf 'datafield #'(lambda (df) (warn "Datafield not removed: ~S" df))))


(defun field-kind-and-data (field-rtf)
  (unless (consp field-rtf)
    (error "Can't find field code in ~S" field-rtf))
  (let ((item (first field-rtf)))
    (cond
     ((consp item)
      (field-kind-and-data item))
     ((stringp item)
      (setq item (string-trim '(#\space) item))
      (let ((i (position #\space item)))
        (values (subseq item 0 i)
                (and i (string-left-trim '(#\space) (subseq item (1+ i)))))))
     (t (field-kind-and-data (rest field-rtf))))))


(defun collect-fields (rtf)
  (let ((fields (make-hash-table :test #'equal)))
    (each-rtf-tag
     rtf 'fldinst
     #'(lambda (rtf)
         (multiple-value-bind (field-kind field-data) (field-kind-and-data rtf)
           (cond
            ((member field-kind '("PAGE" "SAVEDATE" "SEQ" "SYMBOL") :test #'equal))
            ((equal field-kind "REF")
             (when (position #\space field-data)
               (error "Bad REF field: ~S" rtf))
             (incf (gethash field-data fields 0))
             (print field-data))
            (t (error "Unrecognized field kind ~S" field-kind))))))
    fields))


(defun delete-unused-fields (rtf)
  (let ((fields (collect-fields rtf)))
    (labels
      ((delete (rtf rev)
         (if (endp rtf)
           (nreverse rev)
           (let ((first (first rtf))
                 (rest (rest rtf)))
             (cond
              ((and (consp first)
                    (eq (first first) '*)
                    (member (second first) '(bkmkstart bkmkend))
                    (not (gethash (third first) fields)))
               (unless (and (stringp (third first)) (null (cdddr first)))
                 (error "Unknown bookmark format: ~S" first))
               (delete rest rev))
              ((consp first)
               (delete rest (cons (delete first nil) rev)))
              (t (delete rest (cons first rev))))))))
      (delete rtf nil))))
       


(defun delete-unused-rtf-list-overrides (rtf)
  (delete-unused-rtf-data rtf 'ls 'listoverridetable))

(defun delete-unused-rtf-lists (rtf)
  (delete-unused-rtf-data rtf 'listid 'listtable))


#|
(declaim (optimize (debug 1))) ;*****

(setq r (read-rtf-from-local-file "Huit:E4:E4.rtf"))
(delete-unused-rtf-list-overrides r)
(delete-unused-rtf-lists r)
(delete-datafields r)
(setq r (delete-unused-fields r))
(write-rtf-to-local-file ":private:Edition4a.rtf" r)

(setq r (read-rtf-from-local-file ":private:Edition4b.rtf"))


(each-rtf-tag r 'listid #'(lambda (rtf)
                        (assert-true (integerp (second rtf)))
                        (print (list (first rtf) (second rtf)))))

(debug-depict-rtf
 "JavaScript 2 Lexical Grammar"
 #'(lambda (rtf-stream)
     (depict-world-commands rtf-stream *lw* :visible-semantics nil)))
|#
