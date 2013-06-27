/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


// A short test program with which to experiment with the assembler.

//satisfies  CPU(X86_64)
//#define WTF_CPU_X86_64

// satisfies  ENABLE(ASSEMBLER)
#define ENABLE_ASSEMBLER 1

// satisfies  ENABLE(JIT)
#define ENABLE_JIT 1

#define USE_SYSTEM_MALLOC 1
// leads to FORCE_SYSTEM_MALLOC in wtf/FastMalloc.cpp

#include "assembler/jit/ExecutableAllocator.h"
#include "assembler/assembler/LinkBuffer.h"
#include "assembler/assembler/CodeLocation.h"
#include "assembler/assembler/RepatchBuffer.h"

#include "assembler/assembler/MacroAssembler.h"

#include <stdio.h>

/////////////////////////////////////////////////////////////////
// Temporary scaffolding for selecting the arch
#undef ARCH_x86
#undef ARCH_amd64
#undef ARCH_arm

#if defined(__APPLE__) && defined(__i386__)
#  define ARCH_x86 1
#elif defined(__APPLE__) && defined(__x86_64__)
#  define ARCH_amd64 1
#elif defined(__linux__) && defined(__i386__)
#  define ARCH_x86 1
#elif defined(__linux__) && defined(__x86_64__)
#  define ARCH_amd64 1
#elif defined(__linux__) && defined(__arm__)
#  define ARCH_arm 1
#elif defined(_MSC_VER) && defined(_M_IX86)
#  define ARCH_x86 1
#endif
/////////////////////////////////////////////////////////////////

// just somewhere convenient to put a breakpoint, before
// running gdb
#if WTF_COMPILER_GCC
__attribute__((noinline))
#endif
void pre_run ( void ) { }

/////////////////////////////////////////////////////////////////
//// test1 (simple straight line code) 
#if WTF_COMPILER_GCC

void test1 ( void )
{
  printf("\n------------ Test 1 (straight line code) ------------\n\n" );

  // Create new assembler
  JSC::MacroAssembler* am = new JSC::MacroAssembler();

#if defined(ARCH_amd64)
  JSC::X86Registers::RegisterID areg = JSC::X86Registers::r15;
  // dump some instructions into it
  //    xor    %r15,%r15
  //    add    $0x7b,%r15
  //    add    $0x141,%r15
  //    retq
  am->xorPtr(areg,areg);
  am->addPtr(JSC::MacroAssembler::Imm32(123), areg);
  am->addPtr(JSC::MacroAssembler::Imm32(321), areg);
  am->ret();
#endif

#if defined(ARCH_x86)
  JSC::X86Registers::RegisterID areg = JSC::X86Registers::edi;
  // dump some instructions into it
  //    xor    %edi,%edi
  //    add    $0x7b,%edi
  //    add    $0x141,%edi
  //    ret
  am->xorPtr(areg,areg);
  am->addPtr(JSC::MacroAssembler::Imm32(123), areg);
  am->addPtr(JSC::MacroAssembler::Imm32(321), areg);
  am->ret();
#endif

#if defined(ARCH_arm)
  JSC::ARMRegisters::RegisterID areg = JSC::ARMRegisters::r8;
  //    eors    r8, r8, r8
  //    adds    r8, r8, #123    ; 0x7b
  //    mov     r3, #256        ; 0x100
  //    orr     r3, r3, #65     ; 0x41
  //    adds    r8, r8, r3
  //    mov     pc, lr
  am->xorPtr(areg,areg);
  am->addPtr(JSC::MacroAssembler::Imm32(123), areg);
  am->addPtr(JSC::MacroAssembler::Imm32(321), areg);
  am->ret();
#endif

  // prepare a link buffer, into which we can copy the completed insns
  JSC::ExecutableAllocator* eal = new JSC::ExecutableAllocator();

  // intermediate step .. get the pool suited for the size of code in 'am'
  //WTF::PassRefPtr<JSC::ExecutablePool> ep = eal->poolForSize( am->size() );
  JSC::ExecutablePool* ep = eal->poolForSize( am->size() );

  // constructor for LinkBuffer asks ep to allocate r-x memory,
  // then copies it there.
  JSC::LinkBuffer patchBuffer(am, ep, JSC::METHOD_CODE);

  // finalize
  JSC::MacroAssemblerCodeRef cr = patchBuffer.finalizeCode();

  // cr now holds a pointer to the final runnable code.
  void* entry = cr.m_code.executableAddress();

  printf("disas %p %p\n",
         entry, (char*)entry + cr.m_size);
  pre_run();

  unsigned long result = 0x55555555;

#if defined(ARCH_amd64)
  // call the generated piece of code.  It puts its result in r15.
  __asm__ __volatile__(
     "callq *%1"           "\n\t"
     "movq  %%r15, %0"     "\n"
     :/*out*/   "=r"(result)
     :/*in*/    "r"(entry)
     :/*trash*/ "r15","cc"
  );
#endif
#if defined(ARCH_x86)
  // call the generated piece of code.  It puts its result in edi.
  __asm__ __volatile__(
     "calll *%1"           "\n\t"
     "movl  %%edi, %0"     "\n"
     :/*out*/   "=r"(result)
     :/*in*/    "r"(entry)
     :/*trash*/ "edi","cc"
  );
#endif
#if defined(ARCH_arm)
  // call the generated piece of code.  It puts its result in r8.
  __asm__ __volatile__(
     "blx   %1"            "\n\t"
     "mov   %0, %%r8"      "\n"
     :/*out*/   "=r"(result)
     :/*in*/    "r"(entry)
     :/*trash*/ "r8","cc"
  );
#endif

  printf("\n");
  printf("value computed is %lu (expected 444)\n", result);
  printf("\n");

  delete eal;
  delete am;
}

#endif /* WTF_COMPILER_GCC */

/////////////////////////////////////////////////////////////////
//// test2 (a simple counting-down loop) 
#if WTF_COMPILER_GCC

void test2 ( void )
{
  printf("\n------------ Test 2 (mini loop) ------------\n\n" );

  // Create new assembler
  JSC::MacroAssembler* am = new JSC::MacroAssembler();

#if defined(ARCH_amd64)
  JSC::X86Registers::RegisterID areg = JSC::X86Registers::r15;
  //    xor    %r15,%r15
  //    add    $0x7b,%r15
  //    add    $0x141,%r15
  //    sub    $0x1,%r15
  //    mov    $0x0,%r11
  //    cmp    %r11,%r15
  //    jne    0x7ff6d3e6a00e
  //    retq
  // so r15 always winds up being zero
  am->xorPtr(areg,areg);
  am->addPtr(JSC::MacroAssembler::Imm32(123), areg);
  am->addPtr(JSC::MacroAssembler::Imm32(321), areg);

  JSC::MacroAssembler::Label loopHeadLabel(am);
  am->subPtr(JSC::MacroAssembler::Imm32(1), areg);

  JSC::MacroAssembler::Jump j
     = am->branchPtr(JSC::MacroAssembler::NotEqual,
                     areg, JSC::MacroAssembler::ImmPtr(0));
  j.linkTo(loopHeadLabel, am);

  am->ret();
#endif

#if defined(ARCH_x86)
  JSC::X86Registers::RegisterID areg = JSC::X86Registers::edi;
  //    xor    %edi,%edi
  //    add    $0x7b,%edi
  //    add    $0x141,%edi
  //    sub    $0x1,%edi
  //    test   %edi,%edi
  //    jne    0xf7f9700b
  //    ret
  // so edi always winds up being zero
  am->xorPtr(areg,areg);
  am->addPtr(JSC::MacroAssembler::Imm32(123), areg);
  am->addPtr(JSC::MacroAssembler::Imm32(321), areg);

  JSC::MacroAssembler::Label loopHeadLabel(am);
  am->subPtr(JSC::MacroAssembler::Imm32(1), areg);

  JSC::MacroAssembler::Jump j
     = am->branchPtr(JSC::MacroAssembler::NotEqual,
                     areg, JSC::MacroAssembler::ImmPtr(0));
  j.linkTo(loopHeadLabel, am);

  am->ret();
#endif

#if defined(ARCH_arm)
  JSC::ARMRegisters::RegisterID areg = JSC::ARMRegisters::r8;
  //    eors    r8, r8, r8
  //    adds    r8, r8, #123    ; 0x7b
  //    mov     r3, #256        ; 0x100
  //    orr     r3, r3, #65     ; 0x41
  //    adds    r8, r8, r3
  //    subs    r8, r8, #1      ; 0x1
  //    ldr     r3, [pc, #8]    ; 0x40026028
  //    cmp     r8, r3
  //    bne     0x40026014
  //    mov     pc, lr
  //    andeq   r0, r0, r0         // DATA (0)
  //    andeq   r0, r0, r4, lsl r0 // DATA (?? what's this for?)
  // so r8 always winds up being zero
  am->xorPtr(areg,areg);
  am->addPtr(JSC::MacroAssembler::Imm32(123), areg);
  am->addPtr(JSC::MacroAssembler::Imm32(321), areg);

  JSC::MacroAssembler::Label loopHeadLabel(am);
  am->subPtr(JSC::MacroAssembler::Imm32(1), areg);

  JSC::MacroAssembler::Jump j
     = am->branchPtr(JSC::MacroAssembler::NotEqual,
                     areg, JSC::MacroAssembler::ImmPtr(0));
  j.linkTo(loopHeadLabel, am);

  am->ret();
#endif

  // prepare a link buffer, into which we can copy the completed insns
  JSC::ExecutableAllocator* eal = new JSC::ExecutableAllocator();

  // intermediate step .. get the pool suited for the size of code in 'am'
  //WTF::PassRefPtr<JSC::ExecutablePool> ep = eal->poolForSize( am->size() );
  JSC::ExecutablePool* ep = eal->poolForSize( am->size() );

  // constructor for LinkBuffer asks ep to allocate r-x memory,
  // then copies it there.
  JSC::LinkBuffer patchBuffer(am, ep, JSC::METHOD_CODE);

  // finalize
  JSC::MacroAssemblerCodeRef cr = patchBuffer.finalizeCode();

  // cr now holds a pointer to the final runnable code.
  void* entry = cr.m_code.executableAddress();

  printf("disas %p %p\n",
         entry, (char*)entry + cr.m_size);
  pre_run();

  unsigned long result = 0x55555555;

#if defined(ARCH_amd64)
  // call the generated piece of code.  It puts its result in r15.
  __asm__ __volatile__(
     "callq *%1"           "\n\t"
     "movq  %%r15, %0"     "\n"
     :/*out*/   "=r"(result)
     :/*in*/    "r"(entry)
     :/*trash*/ "r15","cc"
  );
#endif
#if defined(ARCH_x86)
  // call the generated piece of code.  It puts its result in edi.
  __asm__ __volatile__(
     "calll *%1"           "\n\t"
     "movl  %%edi, %0"     "\n"
     :/*out*/   "=r"(result)
     :/*in*/    "r"(entry)
     :/*trash*/ "edi","cc"
  );
#endif
#if defined(ARCH_arm)
  // call the generated piece of code.  It puts its result in r8.
  __asm__ __volatile__(
     "blx   %1"            "\n\t"
     "mov   %0, %%r8"      "\n"
     :/*out*/   "=r"(result)
     :/*in*/    "r"(entry)
     :/*trash*/ "r8","cc"
  );
#endif

  printf("\n");
  printf("value computed is %lu (expected 0)\n", result);
  printf("\n");

  delete eal;
  delete am;
}

#endif /* WTF_COMPILER_GCC */

/////////////////////////////////////////////////////////////////
//// test3 (if-then-else) 
#if WTF_COMPILER_GCC

void test3 ( void )
{
  printf("\n------------ Test 3 (if-then-else) ------------\n\n" );

  // Create new assembler
  JSC::MacroAssembler* am = new JSC::MacroAssembler();

#if defined(ARCH_amd64)
  JSC::X86Registers::RegisterID areg = JSC::X86Registers::r15;
  //    mov    $0x64,%r15d
  //    mov    $0x0,%r11
  //    cmp    %r11,%r15
  //    jne    0x7ff6d3e6a024
  //    mov    $0x40,%r15d
  //    jmpq   0x7ff6d3e6a02a
  //    mov    $0x4,%r15d
  //    retq
  // so r15 ends up being 4

  // put a value in reg
  am->move(JSC::MacroAssembler::Imm32(100), areg);

  // test, and conditionally jump to 'else' branch
  JSC::MacroAssembler::Jump jToElse
    = am->branchPtr(JSC::MacroAssembler::NotEqual,
                    areg, JSC::MacroAssembler::ImmPtr(0));

  // 'then' branch
  am->move(JSC::MacroAssembler::Imm32(64), areg);
  JSC::MacroAssembler::Jump jToAfter
    = am->jump();

  // 'else' branch
  JSC::MacroAssembler::Label elseLbl(am);
  am->move(JSC::MacroAssembler::Imm32(4), areg);

  // after
  JSC::MacroAssembler::Label afterLbl(am);

  am->ret();
#endif

#if defined(ARCH_x86)
  JSC::X86Registers::RegisterID areg = JSC::X86Registers::edi;
  //    mov    $0x64,%edi
  //    test   %edi,%edi
  //    jne    0xf7f22017
  //    mov    $0x40,%edi
  //    jmp    0xf7f2201c
  //    mov    $0x4,%edi
  //    ret
  // so edi ends up being 4

  // put a value in reg
  am->move(JSC::MacroAssembler::Imm32(100), areg);

  // test, and conditionally jump to 'else' branch
  JSC::MacroAssembler::Jump jToElse
    = am->branchPtr(JSC::MacroAssembler::NotEqual,
                    areg, JSC::MacroAssembler::ImmPtr(0));

  // 'then' branch
  am->move(JSC::MacroAssembler::Imm32(64), areg);
  JSC::MacroAssembler::Jump jToAfter
    = am->jump();

  // 'else' branch
  JSC::MacroAssembler::Label elseLbl(am);
  am->move(JSC::MacroAssembler::Imm32(4), areg);

  // after
  JSC::MacroAssembler::Label afterLbl(am);

  am->ret();
#endif

#if defined(ARCH_arm)
  JSC::ARMRegisters::RegisterID areg = JSC::ARMRegisters::r8;
  //    mov     r8, #100        ; 0x64
  //    ldr     r3, [pc, #20]   ; 0x40026020
  //    cmp     r8, r3
  //    bne     0x40026018
  //    mov     r8, #64 ; 0x40
  //    b       0x4002601c
  //    mov     r8, #4  ; 0x4
  //    mov     pc, lr
  //    andeq   r0, r0, r0           // DATA
  //    andeq   r0, r0, r8, lsl r0   // DATA
  //    andeq   r0, r0, r12, lsl r0  // DATA
  //    ldr     r3, [r3, -r3]        // DATA
  // so r8 ends up being 4

  // put a value in reg
  am->move(JSC::MacroAssembler::Imm32(100), areg);

  // test, and conditionally jump to 'else' branch
  JSC::MacroAssembler::Jump jToElse
    = am->branchPtr(JSC::MacroAssembler::NotEqual,
                    areg, JSC::MacroAssembler::ImmPtr(0));

  // 'then' branch
  am->move(JSC::MacroAssembler::Imm32(64), areg);
  JSC::MacroAssembler::Jump jToAfter
    = am->jump();

  // 'else' branch
  JSC::MacroAssembler::Label elseLbl(am);
  am->move(JSC::MacroAssembler::Imm32(4), areg);

  // after
  JSC::MacroAssembler::Label afterLbl(am);

  am->ret();
#endif

  // set branch targets appropriately
  jToElse.linkTo(elseLbl, am);
  jToAfter.linkTo(afterLbl, am);

  // prepare a link buffer, into which we can copy the completed insns
  JSC::ExecutableAllocator* eal = new JSC::ExecutableAllocator();

  // intermediate step .. get the pool suited for the size of code in 'am'
  //WTF::PassRefPtr<JSC::ExecutablePool> ep = eal->poolForSize( am->size() );
  JSC::ExecutablePool* ep = eal->poolForSize( am->size() );

  // constructor for LinkBuffer asks ep to allocate r-x memory,
  // then copies it there.
  JSC::LinkBuffer patchBuffer(am, ep, JSC::METHOD_CODE);

  // finalize
  JSC::MacroAssemblerCodeRef cr = patchBuffer.finalizeCode();

  // cr now holds a pointer to the final runnable code.
  void* entry = cr.m_code.executableAddress();

  printf("disas %p %p\n",
         entry, (char*)entry + cr.m_size);
  pre_run();

  unsigned long result = 0x55555555;

#if defined(ARCH_amd64)
  // call the generated piece of code.  It puts its result in r15.
  __asm__ __volatile__(
     "callq *%1"           "\n\t"
     "movq  %%r15, %0"     "\n"
     :/*out*/   "=r"(result)
     :/*in*/    "r"(entry)
     :/*trash*/ "r15","cc"
  );
#endif
#if defined(ARCH_x86)
  // call the generated piece of code.  It puts its result in edi.
  __asm__ __volatile__(
     "calll *%1"           "\n\t"
     "movl  %%edi, %0"     "\n"
     :/*out*/   "=r"(result)
     :/*in*/    "r"(entry)
     :/*trash*/ "edi","cc"
  );
#endif
#if defined(ARCH_arm)
  // call the generated piece of code.  It puts its result in r8.
  __asm__ __volatile__(
     "blx   %1"            "\n\t"
     "mov   %0, %%r8"      "\n"
     :/*out*/   "=r"(result)
     :/*in*/    "r"(entry)
     :/*trash*/ "r8","cc"
  );
#endif

  printf("\n");
  printf("value computed is %lu (expected 4)\n", result);
  printf("\n");

  delete eal;
  delete am;
}

#endif /* WTF_COMPILER_GCC */

/////////////////////////////////////////////////////////////////
//// test4 (callable function) 

void test4 ( void )
{
  printf("\n------------ Test 4 (callable fn) ------------\n\n" );

  // Create new assembler
  JSC::MacroAssembler* am = new JSC::MacroAssembler();

#if defined(ARCH_amd64)
  // ADD FN PROLOGUE/EPILOGUE so as to make a mini-function
  //    push   %rbp
  //    mov    %rsp,%rbp
  //    push   %rbx
  //    push   %r12
  //    push   %r13
  //    push   %r14
  //    push   %r15
  //    xor    %rax,%rax
  //    add    $0x7b,%rax
  //    add    $0x141,%rax
  //    pop    %r15
  //    pop    %r14
  //    pop    %r13
  //    pop    %r12
  //    pop    %rbx
  //    mov    %rbp,%rsp
  //    pop    %rbp
  //    retq
  // callable as a normal function, returns 444

  JSC::X86Registers::RegisterID rreg = JSC::X86Registers::eax;
  am->push(JSC::X86Registers::ebp);
  am->move(JSC::X86Registers::esp, JSC::X86Registers::ebp);
  am->push(JSC::X86Registers::ebx);
  am->push(JSC::X86Registers::r12);
  am->push(JSC::X86Registers::r13);
  am->push(JSC::X86Registers::r14);
  am->push(JSC::X86Registers::r15);

  am->xorPtr(rreg,rreg);
  am->addPtr(JSC::MacroAssembler::Imm32(123), rreg);
  am->addPtr(JSC::MacroAssembler::Imm32(321), rreg);

  am->pop(JSC::X86Registers::r15);
  am->pop(JSC::X86Registers::r14);
  am->pop(JSC::X86Registers::r13);
  am->pop(JSC::X86Registers::r12);
  am->pop(JSC::X86Registers::ebx);
  am->move(JSC::X86Registers::ebp, JSC::X86Registers::esp);
  am->pop(JSC::X86Registers::ebp);
  am->ret();
#endif

#if defined(ARCH_x86)
  // ADD FN PROLOGUE/EPILOGUE so as to make a mini-function
  //    push   %ebp
  //    mov    %esp,%ebp
  //    push   %ebx
  //    push   %esi
  //    push   %edi
  //    xor    %eax,%eax
  //    add    $0x7b,%eax
  //    add    $0x141,%eax
  //    pop    %edi
  //    pop    %esi
  //    pop    %ebx
  //    mov    %ebp,%esp
  //    pop    %ebp
  //    ret
  // callable as a normal function, returns 444

  JSC::X86Registers::RegisterID rreg = JSC::X86Registers::eax;

  am->push(JSC::X86Registers::ebp);
  am->move(JSC::X86Registers::esp, JSC::X86Registers::ebp);
  am->push(JSC::X86Registers::ebx);
  am->push(JSC::X86Registers::esi);
  am->push(JSC::X86Registers::edi);

  am->xorPtr(rreg,rreg);
  am->addPtr(JSC::MacroAssembler::Imm32(123), rreg);
  am->addPtr(JSC::MacroAssembler::Imm32(321), rreg);

  am->pop(JSC::X86Registers::edi);
  am->pop(JSC::X86Registers::esi);
  am->pop(JSC::X86Registers::ebx);
  am->move(JSC::X86Registers::ebp, JSC::X86Registers::esp);
  am->pop(JSC::X86Registers::ebp);
  am->ret();
#endif

#if defined(ARCH_arm)
  // ADD FN PROLOGUE/EPILOGUE so as to make a mini-function
  //    push    {r4}            ; (str r4, [sp, #-4]!)
  //    push    {r5}            ; (str r5, [sp, #-4]!)
  //    push    {r6}            ; (str r6, [sp, #-4]!)
  //    push    {r7}            ; (str r7, [sp, #-4]!)
  //    push    {r8}            ; (str r8, [sp, #-4]!)
  //    push    {r9}            ; (str r9, [sp, #-4]!)
  //    push    {r10}           ; (str r10, [sp, #-4]!)
  //    push    {r11}           ; (str r11, [sp, #-4]!)
  //    eors    r0, r0, r0
  //    adds    r0, r0, #123    ; 0x7b
  //    mov     r3, #256        ; 0x100
  //    orr     r3, r3, #65     ; 0x41
  //    adds    r0, r0, r3
  //    pop     {r11}           ; (ldr r11, [sp], #4)
  //    pop     {r10}           ; (ldr r10, [sp], #4)
  //    pop     {r9}            ; (ldr r9, [sp], #4)
  //    pop     {r8}            ; (ldr r8, [sp], #4)
  //    pop     {r7}            ; (ldr r7, [sp], #4)
  //    pop     {r6}            ; (ldr r6, [sp], #4)
  //    pop     {r5}            ; (ldr r5, [sp], #4)
  //    pop     {r4}            ; (ldr r4, [sp], #4)
  //    mov     pc, lr
  // callable as a normal function, returns 444

  JSC::ARMRegisters::RegisterID rreg = JSC::ARMRegisters::r0;

  am->push(JSC::ARMRegisters::r4);
  am->push(JSC::ARMRegisters::r5);
  am->push(JSC::ARMRegisters::r6);
  am->push(JSC::ARMRegisters::r7);
  am->push(JSC::ARMRegisters::r8);
  am->push(JSC::ARMRegisters::r9);
  am->push(JSC::ARMRegisters::r10);
  am->push(JSC::ARMRegisters::r11);

  am->xorPtr(rreg,rreg);
  am->addPtr(JSC::MacroAssembler::Imm32(123), rreg);
  am->addPtr(JSC::MacroAssembler::Imm32(321), rreg);

  am->pop(JSC::ARMRegisters::r11);
  am->pop(JSC::ARMRegisters::r10);
  am->pop(JSC::ARMRegisters::r9);
  am->pop(JSC::ARMRegisters::r8);
  am->pop(JSC::ARMRegisters::r7);
  am->pop(JSC::ARMRegisters::r6);
  am->pop(JSC::ARMRegisters::r5);
  am->pop(JSC::ARMRegisters::r4);

  am->ret();
#endif

  // prepare a link buffer, into which we can copy the completed insns
  JSC::ExecutableAllocator* eal = new JSC::ExecutableAllocator();

  // intermediate step .. get the pool suited for the size of code in 'am'
  //WTF::PassRefPtr<JSC::ExecutablePool> ep = eal->poolForSize( am->size() );
  JSC::ExecutablePool* ep = eal->poolForSize( am->size() );

  // constructor for LinkBuffer asks ep to allocate r-x memory,
  // then copies it there.
  JSC::LinkBuffer patchBuffer(am, ep, JSC::METHOD_CODE);

  // now fix up any branches/calls
  //JSC::FunctionPtr target = JSC::FunctionPtr::FunctionPtr( &cube );

  // finalize
  JSC::MacroAssemblerCodeRef cr = patchBuffer.finalizeCode();

  // cr now holds a pointer to the final runnable code.
  void* entry = cr.m_code.executableAddress();

  printf("disas %p %p\n",
         entry, (char*)entry + cr.m_size);
  pre_run();

  // call the function
  unsigned long (*fn)(void) = (unsigned long (*)())entry;
  unsigned long result = fn();

  printf("\n");
  printf("value computed is %lu (expected 444)\n", result);
  printf("\n");

  delete eal;
  delete am;
}


/////////////////////////////////////////////////////////////////
//// test5 (call in, out, repatch) 

// a function which we will call from the JIT generated code
unsigned long cube   ( unsigned long x ) { return x * x * x; }
unsigned long square ( unsigned long x ) { return x * x; }

void test5 ( void )
{
  printf("\n--------- Test 5 (call in, out, repatch) ---------\n\n" );

  // Create new assembler
  JSC::MacroAssembler* am = new JSC::MacroAssembler();
  JSC::MacroAssembler::Call cl;
  ptrdiff_t offset_of_call_insn;

#if defined(ARCH_amd64)
  // ADD FN PROLOGUE/EPILOGUE so as to make a mini-function
  // and then call a non-JIT-generated helper from within
  // this code
  //    push   %rbp
  //    mov    %rsp,%rbp
  //    push   %rbx
  //    push   %r12
  //    push   %r13
  //    push   %r14
  //    push   %r15
  //    mov    $0x9,%edi
  //    mov    $0x40187e,%r11
  //    callq  *%r11
  //    pop    %r15
  //    pop    %r14
  //    pop    %r13
  //    pop    %r12
  //    pop    %rbx
  //    mov    %rbp,%rsp
  //    pop    %rbp
  //    retq
  JSC::MacroAssembler::Label startOfFnLbl(am);
  am->push(JSC::X86Registers::ebp);
  am->move(JSC::X86Registers::esp, JSC::X86Registers::ebp);
  am->push(JSC::X86Registers::ebx);
  am->push(JSC::X86Registers::r12);
  am->push(JSC::X86Registers::r13);
  am->push(JSC::X86Registers::r14);
  am->push(JSC::X86Registers::r15);

  // let's compute cube(9).  Move $9 to the first arg reg.
  am->move(JSC::MacroAssembler::Imm32(9), JSC::X86Registers::edi);
  cl = am->JSC::MacroAssembler::call();

  // result is now in %rax.  Leave it ther and just return.

  am->pop(JSC::X86Registers::r15);
  am->pop(JSC::X86Registers::r14);
  am->pop(JSC::X86Registers::r13);
  am->pop(JSC::X86Registers::r12);
  am->pop(JSC::X86Registers::ebx);
  am->move(JSC::X86Registers::ebp, JSC::X86Registers::esp);
  am->pop(JSC::X86Registers::ebp);
  am->ret();

  offset_of_call_insn
     = am->JSC::MacroAssembler::differenceBetween(startOfFnLbl, cl);
  if (0) printf("XXXXXXXX offset = %lu\n", offset_of_call_insn);
#endif

#if defined(ARCH_x86)
  // ADD FN PROLOGUE/EPILOGUE so as to make a mini-function
  // and then call a non-JIT-generated helper from within
  // this code
  //    push   %ebp
  //    mov    %esp,%ebp
  //    push   %ebx
  //    push   %esi
  //    push   %edi
  //    push   $0x9
  //    call   0x80490e9 <_Z4cubem>
  //    add    $0x4,%esp
  //    pop    %edi
  //    pop    %esi
  //    pop    %ebx
  //    mov    %ebp,%esp
  //    pop    %ebp
  //    ret
  JSC::MacroAssembler::Label startOfFnLbl(am);
  am->push(JSC::X86Registers::ebp);
  am->move(JSC::X86Registers::esp, JSC::X86Registers::ebp);
  am->push(JSC::X86Registers::ebx);
  am->push(JSC::X86Registers::esi);
  am->push(JSC::X86Registers::edi);

  // let's compute cube(9).  Push $9 on the stack.
  am->push(JSC::MacroAssembler::Imm32(9));
  cl = am->JSC::MacroAssembler::call();
  am->addPtr(JSC::MacroAssembler::Imm32(4), JSC::X86Registers::esp);
  // result is now in %eax.  Leave it there and just return.

  am->pop(JSC::X86Registers::edi);
  am->pop(JSC::X86Registers::esi);
  am->pop(JSC::X86Registers::ebx);
  am->move(JSC::X86Registers::ebp, JSC::X86Registers::esp);
  am->pop(JSC::X86Registers::ebp);
  am->ret();

  offset_of_call_insn
     = am->JSC::MacroAssembler::differenceBetween(startOfFnLbl, cl);
  if (0) printf("XXXXXXXX offset = %lu\n",
                (unsigned long)offset_of_call_insn);
#endif

#if defined(ARCH_arm)
  // ADD FN PROLOGUE/EPILOGUE so as to make a mini-function
  //    push    {r4}            ; (str r4, [sp, #-4]!)
  //    push    {r5}            ; (str r5, [sp, #-4]!)
  //    push    {r6}            ; (str r6, [sp, #-4]!)
  //    push    {r7}            ; (str r7, [sp, #-4]!)
  //    push    {r8}            ; (str r8, [sp, #-4]!)
  //    push    {r9}            ; (str r9, [sp, #-4]!)
  //    push    {r10}           ; (str r10, [sp, #-4]!)
  //    push    {r11}           ; (str r11, [sp, #-4]!)
  //    eors    r0, r0, r0
  //    adds    r0, r0, #123    ; 0x7b
  //    mov     r3, #256        ; 0x100
  //    orr     r3, r3, #65     ; 0x41
  //    adds    r0, r0, r3
  //    pop     {r11}           ; (ldr r11, [sp], #4)
  //    pop     {r10}           ; (ldr r10, [sp], #4)
  //    pop     {r9}            ; (ldr r9, [sp], #4)
  //    pop     {r8}            ; (ldr r8, [sp], #4)
  //    pop     {r7}            ; (ldr r7, [sp], #4)
  //    pop     {r6}            ; (ldr r6, [sp], #4)
  //    pop     {r5}            ; (ldr r5, [sp], #4)
  //    pop     {r4}            ; (ldr r4, [sp], #4)
  //    mov     pc, lr
  // callable as a normal function, returns 444
  JSC::MacroAssembler::Label startOfFnLbl(am);
  am->push(JSC::ARMRegisters::r4);
  am->push(JSC::ARMRegisters::r5);
  am->push(JSC::ARMRegisters::r6);
  am->push(JSC::ARMRegisters::r7);
  am->push(JSC::ARMRegisters::r8);
  am->push(JSC::ARMRegisters::r9);
  am->push(JSC::ARMRegisters::r10);
  am->push(JSC::ARMRegisters::r11);
  am->push(JSC::ARMRegisters::lr);

  // let's compute cube(9).  Get $9 into r0.
  am->move(JSC::MacroAssembler::Imm32(9), JSC::ARMRegisters::r0);
  cl = am->JSC::MacroAssembler::call();
  // result is now in r0.  Leave it there and just return.

  am->pop(JSC::ARMRegisters::lr);
  am->pop(JSC::ARMRegisters::r11);
  am->pop(JSC::ARMRegisters::r10);
  am->pop(JSC::ARMRegisters::r9);
  am->pop(JSC::ARMRegisters::r8);
  am->pop(JSC::ARMRegisters::r7);
  am->pop(JSC::ARMRegisters::r6);
  am->pop(JSC::ARMRegisters::r5);
  am->pop(JSC::ARMRegisters::r4);
  am->ret();

  offset_of_call_insn
     = am->JSC::MacroAssembler::differenceBetween(startOfFnLbl, cl);
  if (0) printf("XXXXXXXX offset = %lu\n",
                (unsigned long)offset_of_call_insn);
#endif

  // prepare a link buffer, into which we can copy the completed insns
  JSC::ExecutableAllocator* eal = new JSC::ExecutableAllocator();

  // intermediate step .. get the pool suited for the size of code in 'am'
  //WTF::PassRefPtr<JSC::ExecutablePool> ep = eal->poolForSize( am->size() );
  JSC::ExecutablePool* ep = eal->poolForSize( am->size() );

  // constructor for LinkBuffer asks ep to allocate r-x memory,
  // then copies it there.
  JSC::LinkBuffer patchBuffer(am, ep, JSC::METHOD_CODE);

  // now fix up any branches/calls
  JSC::FunctionPtr target = JSC::FunctionPtr::FunctionPtr( &cube );
  patchBuffer.link(cl, target);

  JSC::MacroAssemblerCodeRef cr = patchBuffer.finalizeCode();

  // cr now holds a pointer to the final runnable code.
  void* entry = cr.m_code.executableAddress();

  printf("disas %p %p\n",
         entry, (char*)entry + cr.m_size);


  pre_run();

  printf("\n");

  unsigned long (*fn)() = (unsigned long(*)())entry;
  unsigned long result = fn();

  printf("value computed is %lu (expected 729)\n", result);
  printf("\n");

  // now repatch the call in the JITted code to go elsewhere
  JSC::JITCode jc = JSC::JITCode::JITCode(entry, cr.m_size);
  JSC::CodeBlock cb = JSC::CodeBlock::CodeBlock(jc);

  // the address of the call insn, that we want to prod
  JSC::MacroAssemblerCodePtr cp
     = JSC::MacroAssemblerCodePtr( ((char*)entry) + offset_of_call_insn );

  JSC::RepatchBuffer repatchBuffer(&cb);
  repatchBuffer.relink( JSC::CodeLocationCall(cp),
                        JSC::FunctionPtr::FunctionPtr( &square ));
 
  result = fn();
  printf("value computed is %lu (expected 81)\n", result);
  printf("\n\n");

  delete eal;
  delete am;
}

/////////////////////////////////////////////////////////////////

int main ( void )
{
#if WTF_COMPILER_GCC
  test1();
  test2();
  test3();
#endif
  test4();
  test5();
  return 0;
}
