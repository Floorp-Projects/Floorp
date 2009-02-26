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

    export  pixman_msvc_try_armv6_op

    FUNC_HEADER pixman_msvc_try_armv6_op
    uqadd8 r0,r0,r1
    mov pc,lr
    ENTRY_END
    endp

    end
