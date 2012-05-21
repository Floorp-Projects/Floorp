; This Source Code Form is subject to the terms of the Mozilla Public
; License, v. 2.0. If a copy of the MPL was not distributed with this
; file, You can obtain one at http://mozilla.org/MPL/2.0/.


.CODE

extern fmod:PROC

; This is a workaround for KB982107 (http://support.microsoft.com/kb/982107)
js_myfmod PROC FRAME
  .ENDPROLOG
  fnclex
  jmp  fmod
js_myfmod ENDP

END
