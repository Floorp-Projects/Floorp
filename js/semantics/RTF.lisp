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
                         red 192 green 192 blue 192 ";"))   ;16
    (:black cf 1)
    (:blue cf 2)
    (:turquoise cf 3)
    (:bright-green cf 4)
    (:pink cf 5)
    (:red cf 6)
    (:yellow cf 7)
    (:white cf 8)
    (:dark-blue cf 9)
    (:teal cf 10)
    (:green cf 11)
    (:violet cf 12)
    (:dark-red cf 13)
    (:dark-yellow cf 14)
    (:gray-50 cf 15)
    (:gray-25 cf 16)
    
    
    ;Misc.
    (:spc " ")
    (:tab2 tab)
    (:tab3 tab)
    (:nbhy _)             ;Non-breaking hyphen
    (:8-pt fs 16)
    (:9-pt fs 18)
    (:10-pt fs 20)
    (:12-pt fs 24)
    (:no-language lang 1024)
    (:english-us lang 1033)
    (:english-uk lang 2057)
    
    (:english :english-us)
    
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
    ((:left-double-quote 1) ldblquote)
    ((:right-double-quote 1) rdblquote)
    ((:left-angle-quote 1) u 171 \' 199)
    ((:right-angle-quote 1) u 187 \' 200)
    ((:bottom-10 1) (field (* fldinst "SYMBOL 94 \\f \"Symbol\" \\s 10") (fldrslt :symbol :10-pt)))
    ((:vector-assign-10 2) (field (* fldinst "SYMBOL 172 \\f \"Symbol\" \\s 10") (fldrslt :symbol :10-pt)))
    ((:up-arrow-10 1) (field (* fldinst "SYMBOL 173 \\f \"Symbol\" \\s 10") (fldrslt :symbol :10-pt)))
    ((:function-arrow-10 2) (field (* fldinst "SYMBOL 174 \\f \"Symbol\" \\s 10") (fldrslt :symbol :10-pt)))
    ((:cartesian-product-10 2) (field (* fldinst "SYMBOL 180 \\f \"Symbol\" \\s 10") (fldrslt :symbol :10-pt)))
    ((:identical-10 2) (field (* fldinst "SYMBOL 186 \\f \"Symbol\" \\s 10") (fldrslt :symbol :10-pt)))
    ((:circle-plus-10 2) (field (* fldinst "SYMBOL 197 \\f \"Symbol\" \\s 10") (fldrslt :symbol :10-pt)))
    ((:empty-10 2) (field (* fldinst "SYMBOL 198 \\f \"Symbol\" \\s 10") (fldrslt :symbol :10-pt)))
    ((:intersection-10 1) (field (* fldinst "SYMBOL 199 \\f \"Symbol\" \\s 10") (fldrslt :symbol :10-pt)))
    ((:union-10 1) (field (* fldinst "SYMBOL 200 \\f \"Symbol\" \\s 10") (fldrslt :symbol :10-pt)))
    ((:member-10 2) (field (* fldinst "SYMBOL 206 \\f \"Symbol\" \\s 10") (fldrslt :symbol :10-pt)))
    ((:not-member-10 2) (field (* fldinst "SYMBOL 207 \\f \"Symbol\" \\s 10") (fldrslt :symbol :10-pt)))
    ((:derives-10 2) (field (* fldinst "SYMBOL 222 \\f \"Symbol\" \\s 10") (fldrslt :symbol :10-pt)))
    ((:left-triangle-bracket-10 1) (field (* fldinst "SYMBOL 225 \\f \"Symbol\" \\s 10") (fldrslt :symbol :10-pt)))
    ((:right-triangle-bracket-10 1) (field (* fldinst "SYMBOL 241 \\f \"Symbol\" \\s 10") (fldrslt :symbol :10-pt)))
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
    
    
    ;Styles
    ((+ :rtf-intro) :stylesheet)
    (:stylesheet (stylesheet :styles))
    
    (:normal-num 0)
    (:normal s :normal-num)
    ((+ :styles) (widctlpar :10-pt :english snext :normal-num "Normal;"))
    
    (:body-text-num 1)
    (:body-text s :body-text-num qj sa 120 widctlpar :10-pt :english)
    ((+ :styles) (:body-text sbasedon :normal-num snext :body-text-num "Body Text;"))
    
    (:header-num 2)
    (:header s :header-num nowidctlpar tqr tx 8640 :10-pt :english)
    ((+ :styles) (:header sbasedon :normal-num snext :header-num "header;"))
    
    (:footer-num 3)
    (:footer s :footer-num nowidctlpar tqc tx 4320 :10-pt :english)
    ((+ :styles) (:footer sbasedon :normal-num snext :footer-num "footer;"))
    
    (:section-heading-num 4)
    (:section-heading s :section-heading-num sa 60 keep keepn nowidctlpar hyphpar 0 level 3 b :12-pt :english)
    ((+ :styles) (:section-heading sbasedon :subsection-heading-num snext :body-text-num "heading 3;"))
    
    (:subsection-heading-num 5)
    (:subsection-heading s :subsection-heading-num sa 30 keep keepn nowidctlpar hyphpar 0 level 4 b :10-pt :english)
    ((+ :styles) (:subsection-heading sbasedon :normal-num snext :body-text-num "heading 4;"))
    
    (:grammar-num 10)
    (:grammar s :grammar-num nowidctlpar hyphpar 0 :10-pt :no-language)
    ((+ :styles) (:grammar sbasedon :normal-num snext :grammar-num "Grammar;"))
    
    (:grammar-header-num 11)
    (:grammar-header s :grammar-header-num sb 60 keep keepn nowidctlpar hyphpar 0 b :10-pt :english)
    ((+ :styles) (:grammar-header sbasedon :normal-num snext :grammar-lhs-num "Grammar Header;"))
    
    (:grammar-lhs-num 12)
    (:grammar-lhs s :grammar-lhs-num fi -1440 li 1800 sb 120 keep keepn nowidctlpar hyphpar 0 outlinelevel 4 :10-pt :no-language)
    ((+ :styles) (:grammar-lhs sbasedon :grammar-num snext :grammar-rhs-num "Grammar LHS;"))
    
    (:grammar-lhs-last-num 13)
    (:grammar-lhs-last s :grammar-lhs-last-num fi -1440 li 1800 sb 120 sa 120 keep nowidctlpar hyphpar 0 outlinelevel 4 :10-pt :no-language)
    ((+ :styles) (:grammar-lhs-last sbasedon :grammar-num snext :grammar-lhs-num "Grammar LHS Last;"))
    
    (:grammar-rhs-num 14)
    (:grammar-rhs s :grammar-rhs-num fi -1260 li 1800 keep keepn nowidctlpar tx 720 hyphpar 0 :10-pt :no-language)
    ((+ :styles) (:grammar-rhs sbasedon :grammar-num snext :grammar-rhs-num "Grammar RHS;"))
    
    (:grammar-rhs-last-num 15)
    (:grammar-rhs-last s :grammar-rhs-last-num fi -1260 li 1800 sa 120 keep nowidctlpar tx 720 hyphpar 0 :10-pt :no-language)
    ((+ :styles) (:grammar-rhs-last sbasedon :grammar-rhs-num snext :grammar-lhs-num "Grammar RHS Last;"))
    
    (:grammar-argument-num 16)
    (:grammar-argument s :grammar-argument-num fi -1440 li 1800 sb 120 sa 120 keep nowidctlpar hyphpar 0 outlinelevel 4 :10-pt :no-language)
    ((+ :styles) (:grammar-argument sbasedon :grammar-num snext :grammar-lhs-num "Grammar Argument;"))
    
    (:semantics-num 20)
    (:semantics s :semantics-num li 180 sb 60 sa 60 keep nowidctlpar hyphpar 0 :10-pt :no-language)
    ((+ :styles) (:semantics sbasedon :normal-num snext :semantics-num "Semantics;"))
    
    (:semantics-next-num 21)
    (:semantics-next s :semantics-next-num li 540 sa 60 keep nowidctlpar hyphpar 0 :10-pt :no-language)
    ((+ :styles) (:semantics-next sbasedon :semantics-num snext :semantics-next-num "Semantics Next;"))
    
    (:default-paragraph-font-num 30)
    (:default-paragraph-font cs :default-paragraph-font-num)
    ((+ :styles) (* :default-paragraph-font additive "Default Paragraph Font;"))
    
    (:page-number-num 31)
    (:page-number cs :page-number-num)
    ((+ :styles) (* :page-number additive sbasedon :default-paragraph-font-num "page number;"))
    
    (:character-literal-num 32)
    (:character-literal cs :character-literal-num b :courier :blue :no-language)
    ((+ :styles) (* :character-literal additive sbasedon :default-paragraph-font-num "Character Literal;"))
    
    (:character-literal-control-num 33)
    (:character-literal-control cs :character-literal-control-num b 0 :times :dark-blue)
    ((+ :styles) (* :character-literal-control additive sbasedon :default-paragraph-font-num "Character Literal Control;"))
    
    (:terminal-num 34)
    (:terminal cs :terminal-num b :palatino :teal :no-language)
    ((+ :styles) (* :terminal additive sbasedon :default-paragraph-font-num "Terminal;"))
    
    (:terminal-keyword-num 35)
    (:terminal-keyword cs :terminal-keyword-num b :courier :blue :no-language)
    ((+ :styles) (* :terminal-keyword additive sbasedon :terminal-num "Terminal Keyword;"))
    
    (:nonterminal-num 36)
    (:nonterminal cs :nonterminal-num i :palatino :dark-red :no-language)
    ((+ :styles) (* :nonterminal additive sbasedon :default-paragraph-font-num "Nonterminal;"))
    
    (:nonterminal-attribute-num 37)
    (:nonterminal-attribute cs :nonterminal-attribute-num i 0)
    ((+ :styles) (* :nonterminal-attribute additive sbasedon :default-paragraph-font-num "Nonterminal Attribute;"))
    
    (:nonterminal-argument-num 38)
    (:nonterminal-argument cs :nonterminal-argument-num)
    ((+ :styles) (* :nonterminal-argument additive sbasedon :default-paragraph-font-num "Nonterminal Argument;"))
    
    (:semantic-keyword-num 40)
    (:semantic-keyword cs :semantic-keyword-num b :times)
    ((+ :styles) (* :semantic-keyword additive sbasedon :default-paragraph-font-num "Semantic Keyword;"))
    
    (:type-expression-num 41)
    (:type-expression cs :type-expression-num :times :red :no-language)
    ((+ :styles) (* :type-expression additive sbasedon :default-paragraph-font-num "Type Expression;"))
    
    (:type-name-num 42)
    (:type-name cs :type-name-num scaps :times :red :no-language)
    ((+ :styles) (* :type-name additive sbasedon :type-expression-num "Type Name;"))
    
    (:field-name-num 43)
    (:field-name cs :field-name-num :helvetica :red :no-language)
    ((+ :styles) (* :field-name additive sbasedon :type-expression-num "Field Name;"))
    
    (:global-variable-num 44)
    (:global-variable cs :global-variable-num i :times :green :no-language)
    ((+ :styles) (* :global-variable additive sbasedon :default-paragraph-font-num "Global Variable;"))
    
    (:local-variable-num 45)
    (:local-variable cs :local-variable-num i :times :bright-green :no-language)
    ((+ :styles) (* :local-variable additive sbasedon :default-paragraph-font-num "Local Variable;"))
    
    (:action-name-num 46)
    (:action-name cs :action-name-num :zapf-chancery :violet :no-language)
    ((+ :styles) (* :action-name additive sbasedon :default-paragraph-font-num "Action Name;"))
    
    
    ;Headers and Footers
    (:header-group header :reset-paragraph :header)
    (:footer-group (footer :reset-paragraph :footer tab (field (* fldinst (:page-number " PAGE ")) (fldrslt (:page-number :no-language "1")))))
    
    
    ;Document Formatting
    (:docfmt widowctrl
             ftnbj          ;footnotes at bottom of page
             aenddoc        ;endnotes at end of document
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
    (:superscript super)
    (:plain-subscript b 0 i 0 :subscript)
    ((:action-begin 1) "[")
    ((:action-end 1) "]")
    ((:vector-begin 1) (b "["))
    ((:vector-end 1) (b "]"))
    ((:empty-vector 2) (b "[]"))
    ((:vector-append 2) :circle-plus-10)
    ((:tuple-begin 1) (b :left-triangle-bracket-10))
    ((:tuple-end 1) (b :right-triangle-bracket-10))
    ((:true 4) (:global-variable "true"))
    ((:false 5) (:global-variable "false"))
    ))


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
  (and (char>= char #\a) (char<= char #\z)))


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
     
     (read-group (nested)
       (let ((char (read nested)))
         (case char
           ((nil) nil)
           (#\} (if nested
                  nil
                  (error "Mismatched }")))
           (#\{ (cons
                 (read-group t)
                 (read-group nested)))
           (#\\ (append
                 (read-control)
                 (read-group nested)))
           (#\newline (read-group nested))
           (t (read-text nested (list char))))))
     
     (read-text (nested chars)
       (let ((char (read nested)))
         (case char
           ((nil)
            (list (coerce (nreverse chars) 'string)))
           ((#\{ #\} #\\)
            (cons (coerce (nreverse chars) 'string)
                  (progn
                    (unread-char char stream)
                    (read-group nested))))
           (#\newline (read-text nested chars))
           (t (read-text nested (cons char chars))))))
     
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
     
     (read-control ()
       (let ((char (read)))
         (if (rtf-control-word-char? char)
           (let* ((control-string (read-control-word (list char)))
                  (control-symbol (intern (string-upcase control-string)))
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
                  (control-symbol (intern (string-upcase control-string))))
             (if (eq control-symbol '\')
               (list control-symbol (read-hex 2))
               (list control-symbol))))))
     
     (read-control-word (chars)
       (let ((char (read)))
         (if (rtf-control-word-char? char)
           (read-control-word (cons char chars))
           (progn
             (unread-char char stream)
             (coerce (nreverse chars) 'string))))))
    
    (read-group nil)))


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
           (write first-rtf :stream stream)
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
                       (:constructor allocate-rtf-stream (env head tail level logical-position))
                       (:copier nil)
                       (:predicate rtf-stream?))
  (style nil :type symbol))           ;Current section or paragraph style or nil if none or emitting paragraph contents


(defmethod print-object ((rtf-stream rtf-stream) stream)
  (print-unreadable-object (rtf-stream stream :identity t)
    (write-string "rtf-stream" stream)))


; Make a new, empty, open rtf-stream with the given definitions for its markup-env.
(defun make-rtf-stream (markup-env level &optional logical-position)
  (let ((head (list nil)))
    (allocate-rtf-stream markup-env head head level logical-position)))


; Make a new, empty, open, top-level rtf-stream with the given definitions
; for its markup-env.
(defun make-top-level-rtf-stream (rtf-definitions)
  (let ((head (list nil))
        (markup-env (make-markup-env nil)))
    (markup-env-define-alist markup-env rtf-definitions)
    (allocate-rtf-stream markup-env head head *markup-stream-top-level* nil)))


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


(defun time-to-string (time)
  (multiple-value-bind (second minute hour day month year weekday) (decode-universal-time time)
    (declare (ignore second minute hour))
    (format nil "~A, ~A ~D, ~D"
            (nth weekday '("Monday" "Tuesday" "Wednesday" "Thursday" "Friday" "Saturday" "Sunday"))
            (nth (1- month) '("January" "February" "March" "April" "May" "June" "July" "August" "September" "October" "November" "December"))
            day
            year)))


; Return the header group.
(defun generate-header-group (title time)
  (list :header-group (assert-type title string) 'tab (time-to-string time)))


; Create a top-level rtf-stream and call emitter to emit its contents.
; emitter takes one argument -- an rtf-stream to which it should emit paragraphs.
; Return the top-level rtf-stream.
(defun depict-rtf-top-level (title emitter)
  (let* ((top-rtf-stream (make-top-level-rtf-stream *rtf-definitions*))
         (rtf-stream (make-rtf-stream (markup-stream-env top-rtf-stream) *markup-stream-paragraph-level*))
         (time (get-universal-time)))
    (markup-stream-append1 rtf-stream ':rtf-intro)
    (let ((info (generate-document-info title *rtf-author* *rtf-company* time)))
      (when info
        (markup-stream-append1 rtf-stream info)))
    (markup-stream-append1 rtf-stream ':docfmt)
    (markup-stream-append1 rtf-stream ':reset-section)
    (markup-stream-append1 rtf-stream (generate-header-group title time))
    (markup-stream-append1 rtf-stream ':footer-group)
    (funcall emitter rtf-stream)
    (markup-stream-append1 top-rtf-stream (markup-stream-unexpanded-output rtf-stream))
    top-rtf-stream))


; Create a top-level rtf-stream and call emitter to emit its contents.
; emitter takes one argument -- an rtf-stream to which it should emit paragraphs.
; Write the resulting RTF to the text file with the given name (relative to the
; local directory).
(defun depict-rtf-to-local-file (filename title emitter)
  (let ((top-rtf-stream (depict-rtf-top-level title emitter)))
    (write-rtf-to-local-file filename (markup-stream-output top-rtf-stream)))
  filename)


; Return the markup accumulated in the markup-stream after expanding all of its macros.
; The markup-stream is closed after this function is called.
(defmethod markup-stream-output ((rtf-stream rtf-stream))
  (markup-env-expand (markup-stream-env rtf-stream) (markup-stream-unexpanded-output rtf-stream) nil))


(defmethod depict-block-style-f ((rtf-stream rtf-stream) block-style emitter)
  (declare (ignore block-style))
  (assert-true (= (markup-stream-level rtf-stream) *markup-stream-paragraph-level*))
  (funcall emitter rtf-stream))


(defmethod depict-paragraph-f ((rtf-stream rtf-stream) paragraph-style emitter)
  (assert-true (= (markup-stream-level rtf-stream) *markup-stream-paragraph-level*))
  (assert-true (and paragraph-style (symbolp paragraph-style)))
  (unless (eq paragraph-style (rtf-stream-style rtf-stream))
    (markup-stream-append1 rtf-stream ':reset-paragraph)
    (markup-stream-append1 rtf-stream paragraph-style))
  (setf (rtf-stream-style rtf-stream) nil)
  (setf (markup-stream-level rtf-stream) *markup-stream-content-level*)
  (setf (markup-stream-logical-position rtf-stream) (make-logical-position))
  (prog1
    (funcall emitter rtf-stream)
    (setf (markup-stream-level rtf-stream) *markup-stream-paragraph-level*)
    (setf (rtf-stream-style rtf-stream) paragraph-style)
    (setf (markup-stream-logical-position rtf-stream) nil)
    (markup-stream-append1 rtf-stream ':new-paragraph)))


(defmethod depict-char-style-f ((rtf-stream rtf-stream) char-style emitter)
  (assert-true (>= (markup-stream-level rtf-stream) *markup-stream-content-level*))
  (assert-true (and char-style (symbolp char-style)))
  (let ((inner-rtf-stream (make-rtf-stream (markup-stream-env rtf-stream) *markup-stream-content-level* (markup-stream-logical-position rtf-stream))))
    (markup-stream-append1 inner-rtf-stream char-style)
    (prog1
      (funcall emitter inner-rtf-stream)
      (rtf-stream-append-or-inline-block rtf-stream (markup-stream-unexpanded-output inner-rtf-stream)))))


(defmethod depict-anchor ((rtf-stream rtf-stream) link-prefix link-name duplicate)
  (declare (ignore link-prefix link-name duplicate))
  (assert-true (= (markup-stream-level rtf-stream) *markup-stream-content-level*)))


(defmethod depict-link-reference-f ((rtf-stream rtf-stream) link-prefix link-name external emitter)
  (declare (ignore link-prefix link-name external))
  (assert-true (= (markup-stream-level rtf-stream) *markup-stream-content-level*))
  (funcall emitter rtf-stream))


#|
(setq r (read-rtf-from-local-file "SampleStyles.rtf"))
(write-rtf-to-local-file "Y.rtf" r)
|#
