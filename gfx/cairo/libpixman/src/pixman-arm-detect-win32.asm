    INCLUDE kxarm.h
	
    area pixman_msvc, code, readonly

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

    export  pixman_msvc_try_arm_simd_op

    FUNC_HEADER pixman_msvc_try_arm_simd_op
    ;; I don't think the msvc arm asm knows how to do SIMD insns
    ;; uqadd8 r3,r3,r3
    DCD 0xe6633f93
    mov pc,lr
    ENTRY_END
    endp

    export  pixman_msvc_try_arm_neon_op

    FUNC_HEADER pixman_msvc_try_arm_neon_op
    ;; I don't think the msvc arm asm knows how to do NEON insns
    ;; veor d0,d0,d0
    DCD 0xf3000110
    mov pc,lr
    ENTRY_END
    endp

    end
