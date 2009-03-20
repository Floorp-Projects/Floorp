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

    export  js_arm_try_armv6t2_op

    ;; I'm not smart enough to figure out which flags to pass to armasm to get it
    ;; to understand movt and fmdrr/vmov; the disassembler figures them out just fine!

    FUNC_HEADER js_arm_try_armv6t2_op
    ;; movt r0,#0xFFFF
    DCD 0xE34F0FFF
    mov pc,lr
    ENTRY_END
    endp

    export  js_arm_try_vfp_op

    FUNC_HEADER js_arm_try_vfp_op
    ;; fmdrr d0, r0, r1
    DCD 0xEC410B10
    mov pc,lr
    ENTRY_END
    endp

    end
