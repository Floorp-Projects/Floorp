    INCLUDE kxarm.h

    area js_msvc, code, readonly

    MACRO
    FUNC_HEADER $Name
FuncName    SETS    VBar:CC:"$Name":CC:VBar
PrologName  SETS    VBar:CC:"$Name":CC:"_Prolog":CC:VBar
FuncEndName SETS    VBar:CC:"$Name":CC:"_end":CC:VBar

    AREA |.pdata|,ALIGN=2,PDATA
    DCD	$FuncName
    DCD	(($PrologName-$FuncName)/4) :OR: ((($FuncEndName-$FuncName)/4):SHL:8) :OR: 0x40000000
    AREA $AreaName,CODE,READONLY
    ALIGN	2
    GLOBAL	$FuncName
    EXPORT	$FuncName
$FuncName
    ROUT
$PrologName
    MEND

    ;; -------- Functions to test processor features.
    export  js_arm_try_thumb_op
    export  js_arm_try_armv6t2_op
    export  js_arm_try_armv7_op
    export  js_arm_try_armv6_op
    export  js_arm_try_armv5_op
    export  js_arm_try_vfp_op

    ;; Test for Thumb support.
    FUNC_HEADER js_arm_try_thumb_op
    bx lr
    mov pc, lr
    ENTRY_END
    endp

    ;; I'm not smart enough to figure out which flags to pass to armasm to get it
    ;; to understand movt and fmdrr/vmov; the disassembler figures them out just fine!

    ;; Test for Thumb2 support.
    FUNC_HEADER js_arm_try_armv6t2_op
    ;; movt r0,#0xFFFF
    DCD 0xE34F0FFF
    mov pc,lr
    ENTRY_END
    endp

    ;; Test for VFP support.
    FUNC_HEADER js_arm_try_vfp_op
    ;; fmdrr d0, r0, r1
    DCD 0xEC410B10
    mov pc,lr
    ENTRY_END
    endp

    ;; Tests for each architecture version.

    FUNC_HEADER js_arm_try_armv7_op
    ;; pli pc, #0
    DCD 0xF45FF000
    mov pc, lr
    ENTRY_END
    endp

    FUNC_HEADER js_arm_try_armv6_op
    ;; rev ip, ip
    DCD 0xE6BFCF3C
    mov pc, lr
    ENTRY_END
    endp

    FUNC_HEADER js_arm_try_armv5_op
    ;; clz ip, ip
    DCD 0xE16FCF1C
    mov pc, lr
    ENTRY_END
    endp

    ;; --------

    end
