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


(progn
  (defparameter *rw*
    (generate-world
     "R"
     '((lexer regexp-lexer
              :lr-1
              :regular-expression-pattern
              ((:unicode-character (% every (:text "Any Unicode character")) () t)
               (:unicode-alphanumeric
                (% alphanumeric (:text "Any Unicode alphabetic or decimal digit character (includes ASCII "
                                       (:character-literal #\0) :nbhy (:character-literal #\9) ", "
                                       (:character-literal #\A) :nbhy (:character-literal #\Z) ", and "
                                       (:character-literal #\a) :nbhy (:character-literal #\z) ")"))
                () t)
               (:line-terminator (#?000A #?000D #?2028 #?2029) () t)
               (:non-terminator (- :unicode-character :line-terminator) () t)
               (:decimal-digit (#\0 #\1 #\2 #\3 #\4 #\5 #\6 #\7 #\8 #\9)
                               (($default-action $default-action)
                                (digit-value digit-value)))
               (:octal-digit (#\0 #\1 #\2 #\3 #\4 #\5 #\6 #\7)
                             (($default-action $default-action)
                              (digit-value digit-value)))
               (:zero-to-three (#\0 #\1 #\2 #\3)
                               ((digit-value digit-value)))
               (:four-to-nine (#\4 #\5 #\6 #\7 #\8 #\9)
                              ((digit-value digit-value)))
               (:eight-or-nine (#\8 #\9)
                               ((digit-value digit-value)))
               (:hex-digit (#\0 #\1 #\2 #\3 #\4 #\5 #\6 #\7 #\8 #\9 #\A #\B #\C #\D #\E #\F #\a #\b #\c #\d #\e #\f)
                           ((hex-value digit-value)))
               (:control-letter (++ (#\A #\B #\C #\D #\E #\F #\G #\H #\I #\J #\K #\L #\M #\N #\O #\P #\Q #\R #\S #\T #\U #\V #\W #\X #\Y #\Z)
                                    (#\a #\b #\c #\d #\e #\f #\g #\h #\i #\j #\k #\l #\m #\n #\o #\p #\q #\r #\s #\t #\u #\v #\w #\x #\y #\z))
                                (($default-action $default-action)))
               ((:pattern-character normal) (- :non-terminator (#\^ #\$ #\\ #\. #\* #\+ #\? #\( #\) #\[ #\] #\{ #\} #\|))
                (($default-action $default-action)))
               ((:pattern-character non-decimal-digit) (- (:pattern-character normal) :decimal-digit)
                (($default-action $default-action)))
               ((:pattern-character non-octal-digit) (- (:pattern-character normal) :octal-digit)
                (($default-action $default-action)))
               ((:range-character any normal) (- :non-terminator (#\\ #\]))
                (($default-action $default-action)))
               ((:range-character any non-decimal-digit) (- (:range-character any normal) :decimal-digit)
                (($default-action $default-action)))
               ((:range-character any non-octal-digit) (- (:range-character any normal) :octal-digit)
                (($default-action $default-action)))
               ((:range-character no-caret normal) (- (:range-character any normal) (#\^))
                (($default-action $default-action)))
               ((:range-character no-dash normal) (- (:range-character any normal) (#\-))
                (($default-action $default-action)))
               ((:range-character no-dash non-decimal-digit) (- (:range-character no-dash normal) :decimal-digit)
                (($default-action $default-action)))
               ((:range-character no-dash non-octal-digit) (- (:range-character no-dash normal) :octal-digit)
                (($default-action $default-action)))
               (:identity-escape (- :non-terminator :unicode-alphanumeric)
                                 (($default-action $default-action))))
              (($default-action character identity (*))
               (digit-value integer digit-char-16 ((:global-variable "digitValue") "(" * ")"))))
       
       (%section "Unicode Character Classes")
       (%charclass :unicode-character)
       (%charclass :unicode-alphanumeric)
       (%charclass :line-terminator)
       (%charclass :non-terminator)
       
       (define line-terminators (set character) (set-of character #?000A #?000D #?2028 #?2029))
       (define non-terminators (set character) (character-set-difference (set-of-ranges character #?0000 #?FFFF) line-terminators))
       (define re-whitespaces (set character) (set-of character #?000C #?000A #?000D #?0009 #?000B #\space))
       (define re-digits (set character) (set-of-ranges character #\0 #\9))
       (define re-word-characters (set character) (set-of-ranges character #\0 #\9 #\A #\Z #\a #\z #\_ nil))
       (%print-actions)
       
       
       (%section "Regular Expression Definitions")
       (deftype r-e-input (tuple (str string) (ignore-case boolean) (multiline boolean)))
       (%text :semantics
         "Field " (:field str r-e-input) " is the input string. "
         (:field ignore-case r-e-input) " and " (:field multiline r-e-input) " are the corresponding regular expression flags.")
       
       (deftype r-e-result (oneof (success r-e-match) failure))
       (deftype r-e-match (tuple (end-index integer)
                            (captured (vector capture))))
       (%text :semantics
         "A " (:type r-e-match) " holds an intermediate state during the pattern-matching process. "
         (:field end-index r-e-match)
         " is the index of the next input character to be matched by the next component in a regular expression pattern. "
         "If we are at the end of the pattern, " (:field end-index r-e-match)
         " is one plus the index of the last matched input character. "
         (:field captured r-e-match)
         " is a zero-based array of the strings captured so far by capturing parentheses.")
       
       (deftype capture (oneof (present string)
                               absent))
       (deftype continuation (-> (r-e-match) r-e-result))
       (%text :semantics
         "A " (:type continuation)
         " is a function that attempts to match the remaining portion of the pattern against the input string, "
         "starting at the intermediate state given by its " (:type r-e-match) " argument. "
         "If a match is possible, it returns a " (:field success r-e-result) " result that contains the final "
         (:type r-e-match) " state; if no match is possible, it returns a " (:field failure r-e-result) " result.")
       
       (deftype matcher (-> (r-e-input r-e-match integer continuation) r-e-result))
       (%text :semantics
         "A " (:type matcher)
         " is a function that attempts to match a middle portion of the pattern against the input string, "
         "starting at the intermediate state given by its " (:type r-e-match) " argument. "
         "Since the remainder of the pattern heavily influences whether (and how) a middle portion will match, we "
         "must pass in a " (:type continuation) " function that checks whether the rest of the pattern matched. "
         "If the continuation returns " (:field failure r-e-result) ", the matcher function may call it repeatedly, "
         "trying various alternatives at pattern choice points.")
       (%text :semantics
         "The " (:type r-e-input) " parameter contains the input string and is merely passed down to subroutines. "
         "The " (:type integer) " parameter contains the number of capturing left parentheses seen so far in the "
         "pattern and is used to assign static, consecutive numbers to capturing parentheses.")
       
       (define (sequence-matcher (matcher1 matcher) (paren-count1 integer) (matcher2 matcher)) matcher
         (function ((t r-e-input) (x r-e-match) (p integer) (c continuation))
           (let ((d continuation (function ((y r-e-match))
                                   (matcher2 t y (+ p paren-count1) c))))
             (matcher1 t x p d))))
       (%text :semantics
         (:global sequence-matcher) " returns a " (:type matcher)
         " that matches the concatenation of the patterns matched by "
         (:local matcher1) " and " (:local matcher2) ". "
         (:local paren-count1) " is the number of capturing left parentheses inside "
         (:local matcher2) "'s pattern.")
       
       (define (character-set-matcher (acceptance-set (set character))) matcher   ;*********ignore case?
         (function ((t r-e-input) (x r-e-match) (p integer :unused) (c continuation))
           (let ((i integer (& end-index x))
                 (s string (& str t)))
             (if (= i (length s))
               (oneof failure)
               (if (character-set-member (nth s i) acceptance-set)
                 (c (tuple r-e-match (+ i 1) (& captured x)))
                 (oneof failure))))))
       (%text :semantics
         (:global character-set-matcher) " returns a " (:type matcher)
         " that matches a single input string character. The match succeeds if the character is a member of the "
         (:local acceptance-set) " set of characters (possibly ignoring case).")
       
       (define (character-matcher (ch character)) matcher
         (character-set-matcher (set-of character ch)))
       (%text :semantics
         (:global character-matcher) " returns a " (:type matcher)
         " that matches a single input string character. The match succeeds if the character is the same as "
         (:local ch) " (possibly ignoring case).")
       
       (%print-actions)
       
       
       (%section "Regular Expression Patterns")
       
       (rule :regular-expression-pattern ((exec (-> (r-e-input integer) r-e-result)))
         (production :regular-expression-pattern (:disjunction) regular-expression-pattern-disjunction
           ((exec (t r-e-input) (index integer))
            ((match :disjunction)
             t
             (tuple r-e-match index (fill-capture (count-parens :disjunction)))
             0
             (function ((x r-e-match)) (oneof success x))))))
       
       (%print-actions)
       (define (fill-capture (i integer)) (vector capture)
         (if (= i 0)
           (vector-of capture)
           (append (fill-capture (- i 1)) (vector (oneof absent)))))
       
       
       (%subsection "Disjunctions")
       
       (rule :disjunction ((match matcher) (count-parens integer))
         (production :disjunction ((:alternative normal)) disjunction-one
           (match (match :alternative))
           (count-parens (count-parens :alternative)))
         (production :disjunction ((:alternative normal) #\| :disjunction) disjunction-more
           ((match (t r-e-input) (x r-e-match) (p integer) (c continuation))
            (case ((match :alternative) t x p c)
              ((success y r-e-match) (oneof success y))
              (failure ((match :disjunction) t x (+ p (count-parens :alternative)) c))))
           (count-parens (+ (count-parens :alternative) (count-parens :disjunction)))))
       
       (%print-actions)
       
       
       (%subsection "Quantifiers")
       
       (grammar-argument :lambda normal non-decimal-digit non-octal-digit)
       
       (rule (:alternative :lambda) ((match matcher) (count-parens integer))
         (production (:alternative :lambda) () alternative-none
           ((match (t r-e-input :unused) (x r-e-match) (p integer :unused) (c continuation))
            (c x))
           (count-parens 0))
         (production (:alternative :lambda) (:assertion (:alternative normal)) alternative-assertion
           ((match (t r-e-input) (x r-e-match) (p integer) (c continuation))
            (if ((test-assertion :assertion) t x)
              ((match :alternative) t x p c)
              (oneof failure)))
           (count-parens (count-parens :alternative)))
         (production (:alternative :lambda) ((:ordinary-atom :lambda) (:alternative normal)) alternative-ordinary-atom
           (match (sequence-matcher (match :ordinary-atom) (count-parens :ordinary-atom) (match :alternative)))
           (count-parens (+ (count-parens :ordinary-atom) (count-parens :alternative))))
         (production (:alternative :lambda) (:one-digit-escape (:alternative non-decimal-digit)) alternative-one-digit-escape
           (match (sequence-matcher (match :one-digit-escape) 0 (match :alternative)))
           (count-parens (count-parens :alternative)))
         (production (:alternative :lambda) (:short-octal-escape (:alternative non-octal-digit)) alternative-short-octal-escape
           (match (sequence-matcher (match :short-octal-escape) 0 (match :alternative)))
           (count-parens (count-parens :alternative)))
         (production (:alternative :lambda) ((:atom :lambda) :quantifier (:alternative normal)) alternative-quantified-atom
           (match
            (let ((min integer (minimum :quantifier))
                  (max limit (maximum :quantifier))
                  (lazy boolean (lazy :quantifier)))
              (if (case max
                    ((finite m integer) (< m min))
                    (infinite false))
                (bottom matcher)
                (let ((looper matcher (repeat-matcher (match :atom) min max lazy (count-parens :atom))))
                  (sequence-matcher looper (count-parens :atom) (match :alternative))))))
           (count-parens (+ (count-parens :atom) (count-parens :alternative)))))
       
       
       (rule :quantifier ((minimum integer) (maximum limit) (lazy boolean))
         (production :quantifier (:quantifier-prefix) quantifier-eager
           (minimum (minimum :quantifier-prefix))
           (maximum (maximum :quantifier-prefix))
           (lazy false))
         (production :quantifier (:quantifier-prefix #\?) quantifier-lazy
           (minimum (minimum :quantifier-prefix))
           (maximum (maximum :quantifier-prefix))
           (lazy true)))
       
       (rule :quantifier-prefix ((minimum integer) (maximum limit))
         (production :quantifier-prefix (#\*) quantifier-prefix-zero-or-more
           (minimum 0)
           (maximum (oneof infinite)))
         (production :quantifier-prefix (#\+) quantifier-prefix-one-or-more
           (minimum 1)
           (maximum (oneof infinite)))
         (production :quantifier-prefix (#\?) quantifier-prefix-zero-or-one
           (minimum 0)
           (maximum (oneof finite 1)))
         (production :quantifier-prefix (#\{ :decimal-digits #\}) quantifier-prefix-repeat
           (minimum (integer-value :decimal-digits))
           (maximum (oneof finite (integer-value :decimal-digits))))
         (production :quantifier-prefix (#\{ :decimal-digits #\, #\}) quantifier-prefix-repeat-or-more
           (minimum (integer-value :decimal-digits))
           (maximum (oneof infinite)))
         (production :quantifier-prefix (#\{ :decimal-digits #\, :decimal-digits #\}) quantifier-prefix-repeat-range
           (minimum (integer-value :decimal-digits 1))
           (maximum (oneof finite (integer-value :decimal-digits 2)))))
       
       (rule :decimal-digits ((integer-value integer))
         (production :decimal-digits (:decimal-digit) decimal-digits-first
           (integer-value (digit-value :decimal-digit)))
         (production :decimal-digits (:decimal-digits :decimal-digit) decimal-digits-rest
           (integer-value (+ (* 10 (integer-value :decimal-digits)) (digit-value :decimal-digit)))))
       (%charclass :decimal-digit)
       
       
       (deftype limit (oneof (finite integer) infinite))
       
       (define (reset-parens (x r-e-match) (p integer) (n-parens integer)) r-e-match
         (if (= n-parens 0)
           x
           (let ((y r-e-match (tuple r-e-match (& end-index x)
                                     (set-nth (& captured x) p (oneof absent)))))
             (reset-parens y (+ p 1) (- n-parens 1)))))
       
       (define (repeat-matcher (body matcher) (min integer) (max limit) (lazy boolean) (n-body-parens integer)) matcher
         (function ((t r-e-input) (x r-e-match) (p integer) (c continuation))
           (if (case max
                 ((finite m integer) (= m 0))
                 (infinite false))
             (c x)
             (let ((d continuation (function ((y r-e-match))
                                     (if (and (= min 0)
                                              (and (is infinite max)
                                                   (= (& end-index y) (& end-index x))))
                                       (oneof failure)
                                       (let ((new-min integer (if (= min 0) 0 (- min 1)))
                                             (new-max limit (case max
                                                              ((finite m integer) (oneof finite (- m 1)))
                                                              (infinite (oneof infinite)))))
                                         ((repeat-matcher body new-min new-max lazy n-body-parens) t y p c)))))
                   (xr r-e-match (reset-parens x p n-body-parens)))
               (if (/= min 0)
                 (body t xr p d)
                 (if lazy
                   (case (c x)
                     ((success z r-e-match) (oneof success z))
                     (failure (body t xr p d)))
                   (case (body t xr p d)
                     ((success z r-e-match) (oneof success z))
                     (failure (c x)))))))))
       
       (%print-actions)
       
       
       (%subsection "Assertions")
       
       (rule :assertion ((test-assertion (-> (r-e-input r-e-match) boolean)))
         (production :assertion (#\^) assertion-beginning
           ((test-assertion (t r-e-input :unused) (x r-e-match))
            (= (& end-index x) 0))) ;*********multiline
         (production :assertion (#\$) assertion-end
           ((test-assertion (t r-e-input) (x r-e-match))
            (= (& end-index x) (length (& str t))))) ;*********multiline
         (production :assertion (#\\ #\b) assertion-word-boundary
           ((test-assertion (t r-e-input) (x r-e-match))
            (at-word-boundary (& end-index x) (& str t))))
         (production :assertion (#\\ #\B) assertion-non-word-boundary
           ((test-assertion (t r-e-input) (x r-e-match))
            (not (at-word-boundary (& end-index x) (& str t))))))
       
       (%print-actions)
       
       (define (at-word-boundary (i integer) (s string)) boolean
         (or (= i 0)
             (or (= i (length s))
                 (xor (character-set-member (nth s (- i 1)) re-word-characters)
                      (character-set-member (nth s i) re-word-characters)))))
       
       
       (%section "Atoms")
       
       (rule (:atom :lambda) ((match matcher) (count-parens integer))
         (production (:atom :lambda) ((:ordinary-atom :lambda)) atom-ordinary-atom
           (match (match :ordinary-atom))
           (count-parens (count-parens :ordinary-atom)))
         (production (:atom :lambda) (:one-digit-escape) atom-one-digit-escape
           (match (match :one-digit-escape))
           (count-parens 0))
         (production (:atom :lambda) (:short-octal-escape) atom-short-octal-escape
           (match (match :short-octal-escape))
           (count-parens 0)))
       
       (rule (:ordinary-atom :lambda) ((match matcher) (count-parens integer))
         (production (:ordinary-atom :lambda) (:compound-atom) atom-compound-atom
           (match (match :compound-atom))
           (count-parens (count-parens :compound-atom)))
         (production (:ordinary-atom :lambda) ((:pattern-character :lambda)) atom-pattern-character
           (match (character-matcher ($default-action :pattern-character)))
           (count-parens 0)))
       
       (%charclass (:pattern-character normal))
       (%charclass (:pattern-character non-decimal-digit))
       (%charclass (:pattern-character non-octal-digit))
       
       (rule :compound-atom ((match matcher) (count-parens integer))
         (production :compound-atom (#\( :disjunction #\)) compound-atom-parentheses
           ((match (t r-e-input) (x r-e-match) (p integer) (c continuation))
            (let ((d continuation
                     (function ((y r-e-match))
                       (let ((updated-captured (vector capture)
                                               (set-nth (& captured y) p (oneof present (subseq (& str t) (& end-index x) (- (& end-index y) 1))))))
                         (c (tuple r-e-match (& end-index y) updated-captured))))))
              ((match :disjunction) t x (+ p 1) d)))
           (count-parens (+ (count-parens :disjunction) 1)))
         (production :compound-atom (#\( #\? #\: :disjunction #\)) compound-atom-non-capturing-parentheses
           (match (match :disjunction))
           (count-parens (count-parens :disjunction)))
         (production :compound-atom (#\.) compound-atom-dot
           (match (character-set-matcher non-terminators)) ;******** Check it
           (count-parens 0))
         (production :compound-atom (:character-class) compound-atom-character-class
           ((match (t r-e-input) (x r-e-match) (p integer) (c continuation))
            (let ((a (set character) ((acceptance-set :character-class) (length (& captured x)))))
              ((character-set-matcher a) t x p c)))
           (count-parens 0))
         (production :compound-atom (:character-escape) compound-atom-character-escape
           (match (character-matcher (character-value :character-escape)))
           (count-parens 0))
         (production :compound-atom (:two-digit-escape) compound-atom-two-digit-escape
           (match (match :two-digit-escape))
           (count-parens 0))
         (production :compound-atom (:character-class-escape) compound-atom-character-class-escape
           (match (character-set-matcher (acceptance-set :character-class-escape)))
           (count-parens 0)))
       (%print-actions)
       
       
       (%section "Escapes")
       
       (rule :character-escape ((character-value character))
         (production :character-escape (:control-escape) character-escape-control
           (character-value (character-value :control-escape)))
         (production :character-escape (#\\ #\c :control-letter) character-escape-control-letter
           (character-value (code-to-character (bitwise-and (character-to-code ($default-action :control-letter)) 31))))
         (production :character-escape (:three-digit-escape) character-escape-three-digit
           (character-value (character-value :three-digit-escape)))
         (production :character-escape (:hex-escape) character-escape-hex
           (character-value (character-value :hex-escape)))
         (production :character-escape (#\\ :identity-escape) character-escape-non-escape
           (character-value ($default-action :identity-escape))))
       
       (%charclass :control-letter)
       (%charclass :identity-escape)
       
       (rule :control-escape ((character-value character))
         (production :control-escape (#\\ #\f) control-escape-form-feed (character-value #?000C))
         (production :control-escape (#\\ #\n) control-escape-new-line (character-value #?000A))
         (production :control-escape (#\\ #\r) control-escape-return (character-value #?000D))
         (production :control-escape (#\\ #\t) control-escape-tab (character-value #?0009))
         (production :control-escape (#\\ #\v) control-escape-vertical-tab (character-value #?000B)))
       (%print-actions)
       
       (%subsection "Numeric Escapes")
       
       (rule :one-digit-escape ((match matcher))
         (production :one-digit-escape (#\\ :decimal-digit) one-digit-escape-1
           (match
            (let ((n integer (digit-value :decimal-digit)))
              (if (= n 0)
                (character-matcher #?0000)
                (backreference-matcher n))))))
       
       (rule :short-octal-escape ((character-value (-> (integer) character)) (match matcher))
         (production :short-octal-escape (#\\ :zero-to-three :octal-digit) short-octal-escape-2
           ((character-value (n-capturing-parens integer))
            (let ((n integer (+ (* 10 (digit-value :zero-to-three)) (digit-value :octal-digit))))
              (if (and (>= n 10) (<= n n-capturing-parens))
                (bottom character)
                (code-to-character (+ (* 8 (digit-value :zero-to-three)) (digit-value :octal-digit))))))
           ((match (t r-e-input) (x r-e-match) (p integer) (c continuation))
            (let ((n integer (+ (* 10 (digit-value :zero-to-three)) (digit-value :octal-digit))))
              (if (and (>= n 10) (<= n (length (& captured x))))
                ((backreference-matcher n) t x p c)
                ((character-matcher (code-to-character (+ (* 8 (digit-value :zero-to-three)) (digit-value :octal-digit))))
                 t x p c))))))
       
       (rule :two-digit-escape ((match matcher))
         (production :two-digit-escape (#\\ :zero-to-three :eight-or-nine) two-digit-escape-under-40
           (match (backreference-matcher (+ (* 10 (digit-value :zero-to-three)) (digit-value :eight-or-nine)))))
         (production :two-digit-escape (#\\ :four-to-nine :decimal-digit) two-digit-escape-over-40
           (match (backreference-matcher (+ (* 10 (digit-value :four-to-nine)) (digit-value :decimal-digit))))))
       
       (rule :three-digit-escape ((character-value character))
         (production :three-digit-escape (#\\ :zero-to-three :octal-digit :octal-digit) three-digit-escape-3
           (character-value (code-to-character (+ (+ (* 64 (digit-value :zero-to-three))
                                                     (* 8 (digit-value :octal-digit 1)))
                                                  (digit-value :octal-digit 2))))))
       (%charclass :zero-to-three)
       (%charclass :four-to-nine)
       (%charclass :octal-digit)
       (%charclass :eight-or-nine)
       
       (define (backreference-matcher (n integer)) matcher
         (function ((t r-e-input) (x r-e-match) (p integer :unused) (c continuation))
           (case (nth-backreference x n)
             ((present ref string)
              (let ((i integer (& end-index x))
                    (s string (& str t)))
                (let ((j integer (+ i (length ref))))
                  (if (> j (length s))
                    (oneof failure)
                    (if (string-equal (subseq s i (- j 1)) ref)   ;*********ignore case?
                      (c (tuple r-e-match j (& captured x)))
                      (oneof failure))))))
             (absent (oneof failure)))))
       
       (define (nth-backreference (x r-e-match) (n integer)) capture
         (if (and (> n 0) (<= n (length (& captured x))))
           (nth (& captured x) (- n 1))
           (bottom capture)))
       (%print-actions)
       
       (rule :hex-escape ((character-value character))
         (production :hex-escape (#\\ #\x :hex-digit :hex-digit) hex-escape-2
           (character-value (code-to-character (+ (* 16 (hex-value :hex-digit 1))
                                                  (hex-value :hex-digit 2)))))
         (production :hex-escape (#\\ #\u :hex-digit :hex-digit :hex-digit :hex-digit) hex-escape-4
           (character-value (code-to-character (+ (+ (+ (* 4096 (hex-value :hex-digit 1))
                                                        (* 256 (hex-value :hex-digit 2)))
                                                     (* 16 (hex-value :hex-digit 3)))
                                                  (hex-value :hex-digit 4))))))
       (%charclass :hex-digit)
       (%print-actions)
       
       
       (%subsection "Character Class Escapes")
       
       (rule :character-class-escape ((acceptance-set (set character)))
         (production :character-class-escape (#\\ #\s) character-class-escape-whitespace
           (acceptance-set re-whitespaces))
         (production :character-class-escape (#\\ #\S) character-class-escape-non-whitespace
           (acceptance-set (character-set-difference (set-of-ranges character #?0000 #?FFFF) re-whitespaces)))
         (production :character-class-escape (#\\ #\d) character-class-escape-digit
           (acceptance-set re-digits))
         (production :character-class-escape (#\\ #\D) character-class-escape-non-digit
           (acceptance-set (character-set-difference (set-of-ranges character #?0000 #?FFFF) re-digits)))
         (production :character-class-escape (#\\ #\w) character-class-escape-word
           (acceptance-set re-word-characters))
         (production :character-class-escape (#\\ #\W) character-class-escape-non-word
           (acceptance-set (character-set-difference (set-of-ranges character #?0000 #?FFFF) re-word-characters))))
       (%print-actions)
       
       
       (%section "User-Specified Character Classes")
       
       (grammar-argument :sigma any no-caret no-dash)
       
       (rule :character-class ((acceptance-set (-> (integer) (set character))))
         (production :character-class (#\[ (:class-ranges no-caret) #\]) character-class-positive
           ((acceptance-set (n-capturing-parens integer))
            ((acceptance-set :class-ranges) n-capturing-parens)))
         (production :character-class (#\[ #\^ (:class-ranges any) #\]) character-class-negative
           ((acceptance-set (n-capturing-parens integer))
            (character-set-difference (set-of-ranges character #?0000 #?FFFF) ((acceptance-set :class-ranges) n-capturing-parens)))))
       
       (exclude (:class-ranges no-dash))
       (rule (:class-ranges :sigma) ((acceptance-set (-> (integer) (set character))))
         (production (:class-ranges :sigma) () class-ranges-none
           ((acceptance-set (n-capturing-parens integer :unused))
            (set-of character)))
         (production (:class-ranges :sigma) ((:range-list :sigma normal)) class-ranges-some
           ((acceptance-set (n-capturing-parens integer))
            ((acceptance-set :range-list) n-capturing-parens))))
       
       (exclude (:range-list no-caret non-decimal-digit))
       (exclude (:range-list no-caret non-octal-digit))
       (rule (:range-list :sigma :lambda) ((acceptance-set (-> (integer) (set character))))
         (production (:range-list :sigma :lambda) ((:final-range-atom :sigma :lambda)) range-list-final-range-atom
           ((acceptance-set (n-capturing-parens integer))
            ((acceptance-set :final-range-atom) n-capturing-parens)))
         (production (:range-list :sigma :lambda) ((:ordinary-range-atom :sigma :lambda) (:range-list-suffix normal)) range-list-ordinary-range-atom
           ((acceptance-set (n-capturing-parens integer))
            (let ((a (set character) (acceptance-set :ordinary-range-atom)))
              ((acceptance-set :range-list-suffix) n-capturing-parens a))))
         (production (:range-list :sigma :lambda) (:zero-escape (:range-list-suffix non-decimal-digit)) range-list-zero-escape
           ((acceptance-set (n-capturing-parens integer))
            (let ((a (set character) (set-of character (character-value :zero-escape))))
              ((acceptance-set :range-list-suffix) n-capturing-parens a))))
         (production (:range-list :sigma :lambda) (:short-octal-escape (:range-list-suffix non-octal-digit)) range-list-short-octal-escape
           ((acceptance-set (n-capturing-parens integer))
            (let ((a (set character) (set-of character ((character-value :short-octal-escape) n-capturing-parens))))
              ((acceptance-set :range-list-suffix) n-capturing-parens a)))))
       
       (rule (:range-list-suffix :lambda) ((acceptance-set (-> (integer (set character)) (set character))))
         (production (:range-list-suffix :lambda) ((:range-list no-dash :lambda)) range-list-suffix-list
           ((acceptance-set (n-capturing-parens integer) (low (set character)))
            (character-set-union low ((acceptance-set :range-list) n-capturing-parens))))
         (production (:range-list-suffix :lambda) (#\- (:range-atom any normal)) range-list-final-range
           ((acceptance-set (n-capturing-parens integer) (low (set character)))
            (character-range low ((acceptance-set :range-atom) n-capturing-parens))))
         (production (:range-list-suffix :lambda) (#\- (:ordinary-range-atom any normal) (:range-list any normal)) range-list-suffix-ordinary-range-atom
           ((acceptance-set (n-capturing-parens integer) (low (set character)))
            (let ((range (set character) (character-range low (acceptance-set :ordinary-range-atom))))
              (character-set-union range ((acceptance-set :range-list) n-capturing-parens)))))
         (production (:range-list-suffix :lambda) (#\- :zero-escape (:range-list any non-decimal-digit)) range-list-suffix-zero-escape
           ((acceptance-set (n-capturing-parens integer) (low (set character)))
            (let ((range (set character) (character-range low (set-of character (character-value :zero-escape)))))
              (character-set-union range ((acceptance-set :range-list) n-capturing-parens)))))
         (production (:range-list-suffix :lambda) (#\- :short-octal-escape (:range-list any non-octal-digit)) range-list-suffix-short-octal-escape
           ((acceptance-set (n-capturing-parens integer) (low (set character)))
            (let ((range (set character) (character-range low (set-of character ((character-value :short-octal-escape) n-capturing-parens)))))
              (character-set-union range ((acceptance-set :range-list) n-capturing-parens))))))
       (%print-actions)
       
       (define (character-range (low (set character)) (high (set character))) (set character)
         (if (or (/= (character-set-length low) 1) (/= (character-set-length high) 1))
           (bottom (set character))
           (let ((l character (character-set-min low))
                 (h character (character-set-min high)))
             (if (char<= l h)
               (set-of-ranges character l h)
               (bottom (set character))))))
       
       
       (%subsection "Character Class Range Atoms")
       
       (exclude (:final-range-atom no-caret non-decimal-digit))
       (exclude (:final-range-atom no-caret non-octal-digit))
       (rule (:final-range-atom :sigma :lambda) ((acceptance-set (-> (integer) (set character))))
         (production (:final-range-atom any :lambda) ((:range-atom any :lambda)) final-range-atom-any
           ((acceptance-set (n-capturing-parens integer))
            ((acceptance-set :range-atom) n-capturing-parens)))
         (production (:final-range-atom no-caret :lambda) ((:range-atom no-caret :lambda)) final-range-atom-no-caret
           ((acceptance-set (n-capturing-parens integer))
            ((acceptance-set :range-atom) n-capturing-parens)))
         (production (:final-range-atom no-dash :lambda) ((:range-atom any :lambda)) final-range-atom-no-dash
           ((acceptance-set (n-capturing-parens integer))
            ((acceptance-set :range-atom) n-capturing-parens))))
       
       (exclude (:range-atom no-caret non-decimal-digit))
       (exclude (:range-atom no-caret non-octal-digit))
       (exclude (:range-atom no-dash normal))
       (exclude (:range-atom no-dash non-decimal-digit))
       (exclude (:range-atom no-dash non-octal-digit))
       (rule (:range-atom :sigma :lambda) ((acceptance-set (-> (integer) (set character))))
         (production (:range-atom :sigma :lambda) ((:ordinary-range-atom :sigma :lambda)) range-atom-ordinary
           ((acceptance-set (n-capturing-parens integer :unused))
            (acceptance-set :ordinary-range-atom)))
         (production (:range-atom :sigma :lambda) (:zero-escape) range-atom-zero-escape
           ((acceptance-set (n-capturing-parens integer :unused))
            (set-of character (character-value :zero-escape))))
         (production (:range-atom :sigma :lambda) (:short-octal-escape) range-atom-short-octal-escape
           ((acceptance-set (n-capturing-parens integer))
            (set-of character ((character-value :short-octal-escape) n-capturing-parens)))))
       
       (exclude (:ordinary-range-atom no-caret non-decimal-digit))
       (exclude (:ordinary-range-atom no-caret non-octal-digit))
       (rule (:ordinary-range-atom :sigma :lambda) ((acceptance-set (set character)))
         (production (:ordinary-range-atom :sigma :lambda) ((:range-character :sigma :lambda)) ordinary-range-atom-character
           (acceptance-set (set-of character ($default-action :range-character))))
         (production (:ordinary-range-atom :sigma :lambda) (:range-escape) ordinary-range-atom-range-escape
           (acceptance-set (acceptance-set :range-escape))))
       
       (%charclass (:range-character any normal))
       (%charclass (:range-character any non-decimal-digit))
       (%charclass (:range-character any non-octal-digit))
       (%charclass (:range-character no-caret normal))
       (%charclass (:range-character no-dash normal))
       (%charclass (:range-character no-dash non-decimal-digit))
       (%charclass (:range-character no-dash non-octal-digit))
       
       (rule :range-escape ((acceptance-set (set character)))
         (production :range-escape (#\\ #\b) range-escape-backspace
           (acceptance-set (set-of character #?0008)))
         (production :range-escape (:character-escape) range-escape-character-escape
           (acceptance-set (set-of character (character-value :character-escape))))
         (production :range-escape (:character-class-escape) range-escape-character-class-escape
           (acceptance-set (acceptance-set :character-class-escape))))
       
       (rule :zero-escape ((character-value character))
         (production :zero-escape (#\\ #\0) zero-escape-0
           (character-value #?0000)))
       (%print-actions)
       )))
  
  (defparameter *rl* (world-lexer *rw* 'regexp-lexer))
  (defparameter *rg* (lexer-grammar *rl*)))


(defun run-regexp (regexp input &optional ignore-case multiline)
  (let ((exec (first (lexer-parse *rl* regexp))))
    (dotimes (i (length input) '(failure))
      (let ((result (funcall exec (list input ignore-case multiline) i)))
        (ecase (first result)
          (success
           (return (list* i (subseq input i (second result)) (cddr result))))
          (failure))))))

#|
(progn
  (depict-rtf-to-local-file
   ";JS20;RegExpGrammar.rtf"
   "Regular Expression Grammar"
   #'(lambda (rtf-stream)
       (depict-world-commands rtf-stream *rw* :visible-semantics nil)))
  (depict-rtf-to-local-file
   ";JS20;RegExpSemantics.rtf"
   "Regular Expression Semantics"
   #'(lambda (rtf-stream)
       (depict-world-commands rtf-stream *rw*))))

(progn
  (depict-html-to-local-file
   ";JS20;RegExpGrammar.html"
   "Regular Expression Grammar"
   t
   #'(lambda (html-stream)
       (depict-world-commands html-stream *rw* :visible-semantics nil)))
  (depict-html-to-local-file
   ";JS20;RegExpSemantics.html"
   "Regular Expression Semantics"
   t
   #'(lambda (html-stream)
       (depict-world-commands html-stream *rw*))))

(with-local-output (s ";JS20;RegExpGrammar.txt") (print-lexer *rl* s) (print-grammar *rg* s))

(lexer-pparse *rl* "a+" :trace t)
(lexer-pparse *rl* "[]+" :trace t)
(run-regexp "(0x|0)2" "0x20")
(run-regexp "(a*)b\\1+c" "aabaaaac")
(run-regexp "(a*)b\\1+" "baaaac")
(run-regexp "b(a+)(a+)?(a+)c" "baaaac")
(run-regexp "(((a+)?(b+)?c)*)" "aacbbbcac")
(run-regexp "(\\s\\S\\s)" "aac xa d fds  fds sac")
(run-regexp "(\\s)" "aac xa deac")
(run-regexp "[01234]+aa+" "93-43aabbc")
(run-regexp "[\\101A-ae-]+" "93ABC-@ezy43abc")
(run-regexp "b[ace]+" "baaaacecfe")
(run-regexp "b[^a]+" "baaaabc")

|#

