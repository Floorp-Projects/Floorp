/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=79:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla SpiderMonkey JavaScript 1.9 code, released
 * May 28, 2008.
 *
 * The Initial Developer of the Original Code is
 *   David Anderson <danderson@mozilla.com>
 *   Andreas Gal <gal@uci.edu>
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifdef DEBUG

#include "nanojit.h"

using namespace avmplus;
using namespace nanojit;

typedef void (* JIT_ENTRANCE)(void *a, void *b);

/* :XXX: HACKHACK - we don't ref insImmPtr until we can make LIR changes. */
#define insImmPtr(x) insImm((intptr_t)x)

static GC *gc;

class Test
{
public:
    virtual 
    ~Test()
    {
    }
    
    virtual void 
    Compile(LirWriter *buf) = 0;
    
    virtual void 
    Compare() = 0;
};

class Test1 : public Test
{
public:
    int m_Value;

    virtual void 
    Compile(LirWriter *lirout)
    {
        m_Value = 0;
        lirout->insStorei(lirout->insImm(4), lirout->insImmPtr(&m_Value), 0);
    }

    virtual void 
    Compare()
    {
        printf("Test1 value: %d (== %d)\n", m_Value, 4);
        assert(m_Value == 4);
    }
} s_Test1;

class Test2 : public Test
{
public:
    virtual void 
    Compile(LirWriter *lirout)
    {
        LIns *pOut;
        LIns *pBase;
        LIns *pPart2, *pSols[2];

        m_Values[0] = 1;
        m_Values[1] = 6;
        m_Values[2] = 2;

        pBase = lirout->insImmPtr(&m_Values[0]);
        pPart2 = 
        lirout->ins2(LIR_sub, 
            lirout->ins2(LIR_mul, 
                lirout->insLoadi(pBase, 4), 
                lirout->insLoadi(pBase, 4)), 
            lirout->ins2(LIR_mul, 
                lirout->ins2(LIR_mul, 
                    lirout->insLoadi(pBase, 8), 
                    lirout->insLoadi(pBase, 0)),
                lirout->insImm(4))
            );
        pPart2 = lirout->ins2(LIR_rsh, pPart2, lirout->insImm(2));
        pPart2 = lirout->ins2(LIR_sub, pPart2, lirout->insImm(3));
        pSols[0] = lirout->ins2(LIR_add, lirout->insLoadi(pBase, 4), pPart2);
        pSols[1] = lirout->ins2(LIR_sub, lirout->insLoadi(pBase, 4), pPart2);

        pSols[0] = lirout->ins2(LIR_lsh,
            pSols[0],
            lirout->ins2(LIR_lsh,
                lirout->insLoadi(pBase, 0),
                lirout->insImm(1))
            );
        pSols[1] = lirout->ins2(LIR_lsh,
            pSols[1],
            lirout->ins2(LIR_lsh,
                lirout->insLoadi(pBase, 0),
                lirout->insImm(1))
            );
        pOut = lirout->insImmPtr(&m_Out[0]);
        lirout->insStorei(pSols[0], pOut, 0);
        lirout->insStorei(pSols[1], pOut, 4);
    }

    virtual void 
    Compare()
    {
        printf("Got results (%d, %d) expecting (%d, %d)\n", 
                m_Out[0], m_Out[1], 40, 8);

        assert(m_Out[0] == 40);
        assert(m_Out[1] == 8);
    }

    int m_Out[2];
    int m_Values[3];
} s_Test2;

class Test3 : public Test
{
public:
    virtual void 
    Compile(LirWriter *lirout)
    {
        LIns *pOut;
        LIns *res;

        pOut = lirout->insImmPtr(&m_Out[0]);

        res = lirout->ins1(LIR_neg, lirout->insImm(-1));    
        lirout->insStorei(res, pOut, 0);

        res = lirout->ins2(LIR_or, res, lirout->insImm(6));
        lirout->insStorei(res, pOut, 4);

        res = lirout->ins2(LIR_xor, res, lirout->insImm(1));
        lirout->insStorei(res, pOut, 8);

        res = lirout->ins2(LIR_and, res, lirout->insImm(3));
        lirout->insStorei(res, pOut, 12);

        res = lirout->ins2(LIR_ush, res, lirout->insImm(1));
        lirout->insStorei(res, pOut, 16);
    }

    virtual void Compare()
    {
        printf("Got results (%d, %d, %d, %d, %d) expected (%d, %d, %d, %d, %d)", 
            m_Out[0],
            m_Out[1],
            m_Out[2],
            m_Out[3],
            m_Out[4],
            1, 7, 6, 2, 1);
        assert(m_Out[0] == 1);
        assert(m_Out[1] == 7);
        assert(m_Out[2] == 6);
        assert(m_Out[3] == 2);
        assert(m_Out[4] == 1);
    }

    int m_Out[5];
} s_Test3;

class Test4 : public Test
{
public:
    virtual void 
    Compile(LirWriter *lirout)
    {
        LIns *pIn, *pOut, *res;

        m_In[0] = -1;
        m_In[1] = 6;
        m_In[2] = 1;
        m_In[3] = 3;
        m_In[4] = 1;
        m_In[5] = 9;

        pIn = lirout->insImmPtr(&m_In[0]);
        pOut = lirout->insImmPtr(&m_Out[0]);

        res = lirout->ins1(LIR_neg, lirout->insLoadi(pIn, 0));
        lirout->insStorei(res, pOut, 0);

        res = lirout->ins2(LIR_or, res, lirout->insLoadi(pIn, 4));
        lirout->insStorei(res, pOut, 4);

        res = lirout->ins2(LIR_xor, res, lirout->insLoadi(pIn, 8));
        lirout->insStorei(res, pOut, 8);

        res = lirout->ins2(LIR_and, res, lirout->insLoadi(pIn, 12));
        lirout->insStorei(res, pOut, 12);

        res = lirout->ins2(LIR_ush, res, lirout->insLoadi(pIn, 16));
        lirout->insStorei(res, pOut, 16);

        res = lirout->ins2(LIR_add, res, lirout->insImm(9));
        lirout->insStorei(res, pOut, 20);
    }

    virtual void 
    Compare()
    {
        printf("Got results (%d, %d, %d, %d, %d, %d) expected (%d, %d, %d, %d, %d, %d)", 
            m_Out[0],
            m_Out[1],
            m_Out[2],
            m_Out[3],
            m_Out[4],
            m_Out[5],
            1, 7, 6, 2, 1, 10);
        assert(m_Out[0] == 1);
        assert(m_Out[1] == 7);
        assert(m_Out[2] == 6);
        assert(m_Out[3] == 2);
        assert(m_Out[4] == 1);
        assert(m_Out[5] == 10);
    }

    int m_In[6];
    int m_Out[6];
} s_Test4;

class Test5 : public Test
{
public: 
    virtual void 
    Compile(LirWriter *lirout)
    {
        LIns *pTop;
        LIns *pAns[2];
        LIns *pIn, *pOut;

        m_In[0] = 1.0f;
        m_In[1] = -6.0f;
        m_In[2] = -27.0f;
        m_ConstFour = 4.0f;
        m_ConstTwelve = 12.0f;
        m_ConstTwo = 2.0f;

        pIn = lirout->insImmPtr(&m_In[0]);
        pOut = lirout->insImmPtr(&m_Out[0]);

        pTop = lirout->ins2(LIR_fmul,   
            lirout->insLoad(LIR_ldq, 
                lirout->insImmPtr(&m_ConstFour),
                0),
            lirout->ins2(LIR_fmul,
                lirout->insLoad(LIR_ldq, pIn, 0*8),
                lirout->insLoad(LIR_ldq, pIn, 2*8))
            );
        pTop = lirout->ins2(LIR_fsub,
            lirout->ins2(LIR_fmul,
                lirout->insLoad(LIR_ldq, pIn, 1*8),
                lirout->insLoad(LIR_ldq, pIn, 1*8)),
            pTop);
        pTop = lirout->ins2(LIR_fdiv,
            pTop,
            lirout->insLoad(LIR_ldq,
                lirout->insImmPtr(&m_ConstTwelve),
                0*8)
            );

        pAns[0] = lirout->ins2(LIR_fadd, 
            lirout->insLoad(LIR_ldq, pIn, 1*8),
            pTop);
        pAns[1] = lirout->ins2(LIR_fsub,
            lirout->insLoad(LIR_ldq, pIn, 1*8),
            pTop);

        pTop = lirout->ins2(LIR_fmul,
            lirout->insLoad(LIR_ldq,
                lirout->insImmPtr(&m_ConstTwo),
                0*8),
            lirout->insLoad(LIR_ldq, pIn, 0*8));
        pAns[0] = lirout->ins2(LIR_fdiv, pAns[0], pTop);
        pAns[1] = lirout->ins2(LIR_fdiv, pAns[1], pTop);

        lirout->insStorei(pAns[0], pOut, 0*8);
        lirout->insStorei(pAns[1], pOut, 1*8);
    }

    virtual void Compare()
    {
        printf("Result of %f*x^2 + %f*x + %f: (x + %f)(x + %f)\n",
            m_In[0],
            m_In[1],
            m_In[2],
            m_Out[0],
            m_Out[1]);
        printf("Expected 3 and -9!\n");
        assert(m_Out[0] == 3.0f);
        assert(m_Out[1] == -9.0f);
    }

    double m_ConstTwo;
    double m_ConstTwelve;
    double m_ConstFour;
    double m_In[3];
    double m_Out[2];
} s_Test5;

void
do_test(Test* test)
{
    NIns *code;
	SideExit exit;
    Fragment *frag;
    Fragmento *frago;
    JIT_ENTRANCE jit;
    LirBuffer *lirbuf;
    LirBufWriter *lirout;
    InterpState state;
    avmplus::AvmCore *core;

    core = new avmplus::AvmCore();
    
    /* Initialize our dummy interpreter state */
    frago = new (gc) nanojit::Fragmento(core);

    state.ip = NULL;
    state.sp = NULL;

    /* Begin a dummy trace */
	frago->labels = new (gc) LabelMap(core, NULL);
    frag = frago->getLoop(state.ip);
    lirbuf = new (gc) LirBuffer(frago, NULL);
	lirbuf->names = new (gc) LirNameMap(gc, NULL, frago->labels);
    frag->lirbuf = lirbuf;
    lirout = new LirBufWriter(lirbuf);
	lirout->ins0(LIR_trace);
    frag->state = lirout->insImm8(LIR_param, Assembler::argRegs[0], 0);
    frag->param1 = lirout->insImm8(LIR_param, Assembler::argRegs[1], 0);
    test->Compile(lirout);
    memset(&exit, 0, sizeof(exit));
    exit.from = frag;
	frag->lastIns = lirout->insGuard(LIR_x, NULL, &exit);
    compile(frago->assm(), frag);
    code = frag->code();
    jit = (JIT_ENTRANCE)code;
    jit(NULL, NULL);
    test->Compare();
}

void 
nanojit_test(void)
{
    do_test(&s_Test1);
    do_test(&s_Test2);
    do_test(&s_Test3);
    do_test(&s_Test4);
}

#endif
