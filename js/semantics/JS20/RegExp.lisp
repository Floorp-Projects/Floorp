;;; The contents of this file are subject to the Netscape Public License
;;; Version 1.0 (the "NPL"); you may not use this file except in
;;; compliance with the NPL.  You may obtain a copy of the NPL at
;;; http://www.mozilla.org/NPL/
;;;
;;; Software distributed under the NPL is distributed on an "AS IS" basis,
;;; WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
;;; for the specific language governing rights and limitations under the
;;; NPL.
;;;
;;; The Initial Developer of this code under the NPL is Netscape
;;; Communications Corporation.  Portions created by Netscape are
;;; Copyright (C) 1998 Netscape Communications Corporation.  All Rights
;;; Reserved.

;;;
;;; ECMAScript sample lexer
;;;
;;; Waldemar Horwat (waldemar@netscape.com)
;;;

(defun digit-char-16 (char)
  (assert-non-null (digit-char-p char 16)))


(progn
  (defparameter *rw*
    (generate-world
     "R"
     '((lexer regexp-lexer
              :lr-1
              :regular-expression-pattern
              ((:unicode-character (% every (:english "Any Unicode character")))
               (:white-space-character (#?0009 #?000B #?000C #\space) ())
               (:line-terminator (#?000A #?000D) ())
               (:regular-pattern-character (- :unicode-character :line-terminator (#\^ #\$ #\\ #\. #\* #\+ #\? #\( #\) #\[ #\] #\|)) ())
               (:pattern-character-no-open-brace (- :regular-pattern-character (#\{)) ())
               (:pattern-character-no-digit-or-open-brace (- :pattern-character-no-open-brace (#\0 #\1 #\2 #\3 #\4 #\5 #\6 #\7 #\8 #\9)) ())
               (:pattern-character-no-digit-or-comma-or-braces (- :pattern-character-no-digit-or-open-brace (#\, #\})) ())
               (:pattern-character-no-digit-or-braces (- :pattern-character-no-digit-or-open-brace (#\})) ())
               
               (:decimal-digit (#\0 #\1 #\2 #\3 #\4 #\5 #\6 #\7 #\8 #\9)
                               ((character-value character-value)
                                (decimal-value digit-value)))
               (:non-zero-digit (#\1 #\2 #\3 #\4 #\5 #\6 #\7 #\8 #\9)
                                ((decimal-value digit-value)))
               (:octal-digit (#\0 #\1 #\2 #\3 #\4 #\5 #\6 #\7)
                             ((character-value character-value)
                              (octal-value digit-value)))
               (:zero-to-three (#\0 #\1 #\2 #\3)
                               ((octal-value digit-value)))
               (:four-to-seven (#\4 #\5 #\6 #\7)
                               ((octal-value digit-value)))
               (:hex-digit (#\0 #\1 #\2 #\3 #\4 #\5 #\6 #\7 #\8 #\9 #\A #\B #\C #\D #\E #\F #\a #\b #\c #\d #\e #\f)
                           ((hex-value digit-value)))
               (:hex-indicator (#\X #\x) ()))
              ((character-value character identity (*))
               (digit-value integer digit-char-16 ((:global-variable "digitValue") "(" * ")"))))
       
       (%section "Regular Expression Patterns")
       (production :regular-expression-pattern ((:alternative normal)) regular-expression-pattern-one)
       (production :regular-expression-pattern ((:alternative normal) #\| :regular-expression-pattern) regular-expression-pattern-more)
       
       (grammar-argument :lambda normal no-open-brace no-digit-or-open-brace no-digit-or-comma-or-braces no-digit-or-braces)
       
       (production (:alternative :lambda) () alternative-none)
       (production (:alternative :lambda) (:assertion (:alternative normal)) alternative-assertion)
       (production (:alternative :lambda) ((:atom :lambda) (:atom-suffix no-open-brace)) alternative-atom)
       
       (production (:atom-suffix :lambda) ((:alternative :lambda)) atom-suffix-alternative)
       (production (:atom-suffix :lambda) (:quantifier-suffix) atom-suffix-quantifier)
       
       (production :quantifier-suffix (#\* (:alternative normal)) quantifier-suffix-zero-or-more)
       (production :quantifier-suffix (#\+ (:alternative normal)) quantifier-suffix-one-or-more)
       (production :quantifier-suffix (#\? (:alternative normal)) quantifier-suffix-zero-or-one)
       (production :quantifier-suffix (#\* #\? (:alternative normal)) quantifier-suffix-minimal-zero-or-more)
       (production :quantifier-suffix (#\+ #\? (:alternative normal)) quantifier-suffix-minimal-one-or-more)
       (production :quantifier-suffix (#\{ :decimal-digits #\} (:alternative normal)) quantifier-suffix-repeat)
       (production :quantifier-suffix (#\{ :decimal-digits #\, #\} (:alternative normal)) quantifier-suffix-repeat-or-more)
       (production :quantifier-suffix (#\{ :decimal-digits #\, :decimal-digits #\} (:alternative normal)) quantifier-suffix-repeat-range)
       (production :quantifier-suffix (#\{ (:atom-suffix no-digit-or-open-brace)) quantifier-suffix-fake-repeat-1)
       (production :quantifier-suffix (#\{ :decimal-digits (:atom-suffix no-digit-or-comma-or-braces)) quantifier-suffix-fake-repeat-2)
       (production :quantifier-suffix (#\{ :decimal-digits #\, (:atom-suffix no-digit-or-braces)) quantifier-suffix-fake-repeat-3)
       (production :quantifier-suffix (#\{ :decimal-digits #\, :decimal-digits (:atom-suffix no-digit-or-braces)) quantifier-suffix-fake-repeat-4)
       
       (production :assertion (#\^) assertion-beginning)
       (production :assertion (#\$) assertion-end)
       ;(production :assertion (#\\ #\b) assertion-word-boundary)
       ;(production :assertion (#\\ #\B) assertion-non-word-boundary)
       
       (production :decimal-digits (:decimal-digit) decimal-digits-first)
       (production :decimal-digits (:decimal-digits :decimal-digit) decimal-digits-rest)
       
       (%charclass :decimal-digit)
       
       (production (:atom :lambda) (:compound-atom) atom-compound-atom)
       (production (:atom normal) (:regular-pattern-character) atom-regular-pattern-character)
       (production (:atom no-open-brace) (:pattern-character-no-open-brace) atom-pattern-character-no-open-brace)
       (production (:atom no-digit-or-open-brace) (:pattern-character-no-digit-or-open-brace) atom-pattern-character-no-digit-or-open-brace)
       (production (:atom no-digit-or-comma-or-braces) (:pattern-character-no-digit-or-comma-or-braces) atom-pattern-character-no-digit-or-comma-or-braces)
       (production (:atom no-digit-or-braces) (:pattern-character-no-digit-or-braces) atom-pattern-character-no-digit-or-braces)
       
       (%charclass :regular-pattern-character)
       (%charclass :pattern-character-no-open-brace)
       (%charclass :pattern-character-no-digit-or-open-brace)
       (%charclass :pattern-character-no-digit-or-comma-or-braces)
       (%charclass :pattern-character-no-digit-or-braces)
       
       (production :compound-atom (#\( :regular-expression-pattern #\)) compound-atom-parentheses)
       (production :compound-atom (#\.) compound-atom-dot)
       
       (%print-actions)
       
       (%section "Regular Expression Literals")
       
       (lexer regexp-literal-lexer
              :lalr-1
              :regular-expression-literal
              ((:unicode-character (% every (:english "Any Unicode character")))
               (:line-terminator (#?000A #?000D) ())
               (:non-terminator (- :unicode-character :line-terminator) ())
               (:plain-pattern-char (- :unicode-character :line-terminator (#\\ #\/)) ((character-value character-value)))
               (:letter (++ (#\A #\B #\C #\D #\E #\F #\G #\H #\I #\J #\K #\L #\M #\N #\O #\P #\Q #\R #\S #\T #\U #\V #\W #\X #\Y #\Z)
                            (#\a #\b #\c #\d #\e #\f #\g #\h #\i #\j #\k #\l #\m #\n #\o #\p #\q #\r #\s #\t #\u #\v #\w #\x #\y #\z))
                        ((character-value character-value))))
              ((character-value character identity (*))))
       
       (production :regular-expression-literal (/ :general-pattern / :pattern-flags) regular-expression-literal)
       
       (production :general-pattern (:general-pattern-char) general-pattern-one)
       (production :general-pattern (:general-pattern :general-pattern-char) general-pattern-more)
       
       (production :general-pattern-char (:plain-pattern-char) general-pattern-char-plain)
       (production :general-pattern-char (#\\ :non-terminator) general-pattern-char-escape)
       
       (production :pattern-flags () pattern-flags-none)
       (production :pattern-flags (:pattern-flags :letter) pattern-flags-more)
       
       (%charclass :line-terminator)
       (%charclass :non-terminator)
       (%charclass :plain-pattern-char)
       (%charclass :letter)
       )))
  
  (defparameter *rl* (world-lexer *rw* 'regexp-lexer))
  (defparameter *rg* (lexer-grammar *rl*))
  
  (defparameter *rll* (world-lexer *rw* 'regexp-literal-lexer))
  (defparameter *rlg* (lexer-grammar *rll*))
  (set-up-lexer-metagrammar *rll*)
  (defparameter *rlm* (lexer-metagrammar *rll*)))

#|
(depict-rtf-to-local-file
 ";JS20;RegExpGrammar.rtf"
 "Regular Expression Grammar"
 #'(lambda (rtf-stream)
     (depict-world-commands rtf-stream *rw*)))

(depict-html-to-local-file
 ";JS20;RegExpGrammar.html"
 "Regular Expression Grammar"
 t
 #'(lambda (rtf-stream)
     (depict-world-commands rtf-stream *rw*)))

(with-local-output (s ";JS20;RegExpGrammar.txt") (print-lexer *rl* s) (print-grammar *rg* s))

(print-illegal-strings m)
|#

