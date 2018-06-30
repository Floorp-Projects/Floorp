//
// Copyright (c) 2002-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "compiler/translator/Compiler.h"

#include <sstream>

#include "angle_gl.h"
#include "common/utilities.h"
#include "compiler/translator/CallDAG.h"
#include "compiler/translator/CollectVariables.h"
#include "compiler/translator/Initialize.h"
#include "compiler/translator/IsASTDepthBelowLimit.h"
#include "compiler/translator/OutputTree.h"
#include "compiler/translator/ParseContext.h"
#include "compiler/translator/ValidateLimitations.h"
#include "compiler/translator/ValidateMaxParameters.h"
#include "compiler/translator/ValidateOutputs.h"
#include "compiler/translator/ValidateVaryingLocations.h"
#include "compiler/translator/VariablePacker.h"
#include "compiler/translator/tree_ops/AddAndTrueToLoopCondition.h"
#include "compiler/translator/tree_ops/ClampFragDepth.h"
#include "compiler/translator/tree_ops/ClampPointSize.h"
#include "compiler/translator/tree_ops/DeclareAndInitBuiltinsForInstancedMultiview.h"
#include "compiler/translator/tree_ops/DeferGlobalInitializers.h"
#include "compiler/translator/tree_ops/EmulateGLFragColorBroadcast.h"
#include "compiler/translator/tree_ops/EmulatePrecision.h"
#include "compiler/translator/tree_ops/FoldExpressions.h"
#include "compiler/translator/tree_ops/InitializeVariables.h"
#include "compiler/translator/tree_ops/PruneEmptyCases.h"
#include "compiler/translator/tree_ops/PruneNoOps.h"
#include "compiler/translator/tree_ops/RegenerateStructNames.h"
#include "compiler/translator/tree_ops/RemoveArrayLengthMethod.h"
#include "compiler/translator/tree_ops/RemoveInvariantDeclaration.h"
#include "compiler/translator/tree_ops/RemovePow.h"
#include "compiler/translator/tree_ops/RemoveUnreferencedVariables.h"
#include "compiler/translator/tree_ops/RewriteDoWhile.h"
#include "compiler/translator/tree_ops/RewriteRepeatedAssignToSwizzled.h"
#include "compiler/translator/tree_ops/ScalarizeVecAndMatConstructorArgs.h"
#include "compiler/translator/tree_ops/SeparateDeclarations.h"
#include "compiler/translator/tree_ops/SimplifyLoopConditions.h"
#include "compiler/translator/tree_ops/SplitSequenceOperator.h"
#include "compiler/translator/tree_ops/UnfoldShortCircuitAST.h"
#include "compiler/translator/tree_ops/UseInterfaceBlockFields.h"
#include "compiler/translator/tree_ops/VectorizeVectorScalarArithmetic.h"
#include "compiler/translator/tree_util/BuiltIn_autogen.h"
#include "compiler/translator/tree_util/IntermNodePatternMatcher.h"
#include "compiler/translator/util.h"
#include "third_party/compiler/ArrayBoundsClamper.h"

namespace sh
{

namespace
{

#if defined(ANGLE_ENABLE_FUZZER_CORPUS_OUTPUT)
void DumpFuzzerCase(char const *const *shaderStrings,
                    size_t numStrings,
                    uint32_t type,
                    uint32_t spec,
                    uint32_t output,
                    uint64_t options)
{
    static int fileIndex = 0;

    std::ostringstream o;
    o << "corpus/" << fileIndex++ << ".sample";
    std::string s = o.str();

    // Must match the input format of the fuzzer
    FILE *f = fopen(s.c_str(), "w");
    fwrite(&type, sizeof(type), 1, f);
    fwrite(&spec, sizeof(spec), 1, f);
    fwrite(&output, sizeof(output), 1, f);
    fwrite(&options, sizeof(options), 1, f);

    char zero[128 - 20] = {0};
    fwrite(&zero, 128 - 20, 1, f);

    for (size_t i = 0; i < numStrings; i++)
    {
        fwrite(shaderStrings[i], sizeof(char), strlen(shaderStrings[i]), f);
    }
    fwrite(&zero, 1, 1, f);

    fclose(f);
}
#endif  // defined(ANGLE_ENABLE_FUZZER_CORPUS_OUTPUT)
}  // anonymous namespace

bool IsWebGLBasedSpec(ShShaderSpec spec)
{
    return (spec == SH_WEBGL_SPEC || spec == SH_WEBGL2_SPEC || spec == SH_WEBGL3_SPEC);
}

bool IsGLSL130OrNewer(ShShaderOutput output)
{
    return (output == SH_GLSL_130_OUTPUT || output == SH_GLSL_140_OUTPUT ||
            output == SH_GLSL_150_CORE_OUTPUT || output == SH_GLSL_330_CORE_OUTPUT ||
            output == SH_GLSL_400_CORE_OUTPUT || output == SH_GLSL_410_CORE_OUTPUT ||
            output == SH_GLSL_420_CORE_OUTPUT || output == SH_GLSL_430_CORE_OUTPUT ||
            output == SH_GLSL_440_CORE_OUTPUT || output == SH_GLSL_450_CORE_OUTPUT);
}

bool IsGLSL420OrNewer(ShShaderOutput output)
{
    return (output == SH_GLSL_420_CORE_OUTPUT || output == SH_GLSL_430_CORE_OUTPUT ||
            output == SH_GLSL_440_CORE_OUTPUT || output == SH_GLSL_450_CORE_OUTPUT);
}

bool IsGLSL410OrOlder(ShShaderOutput output)
{
    return (output == SH_GLSL_130_OUTPUT || output == SH_GLSL_140_OUTPUT ||
            output == SH_GLSL_150_CORE_OUTPUT || output == SH_GLSL_330_CORE_OUTPUT ||
            output == SH_GLSL_400_CORE_OUTPUT || output == SH_GLSL_410_CORE_OUTPUT);
}

bool RemoveInvariant(sh::GLenum shaderType,
                     int shaderVersion,
                     ShShaderOutput outputType,
                     ShCompileOptions compileOptions)
{
    if ((compileOptions & SH_DONT_REMOVE_INVARIANT_FOR_FRAGMENT_INPUT) == 0 &&
        shaderType == GL_FRAGMENT_SHADER && IsGLSL420OrNewer(outputType))
        return true;

    if ((compileOptions & SH_REMOVE_INVARIANT_AND_CENTROID_FOR_ESSL3) != 0 &&
        shaderVersion >= 300 && shaderType == GL_VERTEX_SHADER)
        return true;

    return false;
}

size_t GetGlobalMaxTokenSize(ShShaderSpec spec)
{
    // WebGL defines a max token length of 256, while ES2 leaves max token
    // size undefined. ES3 defines a max size of 1024 characters.
    switch (spec)
    {
        case SH_WEBGL_SPEC:
            return 256;
        default:
            return 1024;
    }
}

int GetMaxUniformVectorsForShaderType(GLenum shaderType, const ShBuiltInResources &resources)
{
    switch (shaderType)
    {
        case GL_VERTEX_SHADER:
            return resources.MaxVertexUniformVectors;
        case GL_FRAGMENT_SHADER:
            return resources.MaxFragmentUniformVectors;

        // TODO (jiawei.shao@intel.com): check if we need finer-grained component counting
        case GL_COMPUTE_SHADER:
            return resources.MaxComputeUniformComponents / 4;
        case GL_GEOMETRY_SHADER_EXT:
            return resources.MaxGeometryUniformComponents / 4;
        default:
            UNREACHABLE();
            return -1;
    }
}

namespace
{

class TScopedPoolAllocator
{
  public:
    TScopedPoolAllocator(TPoolAllocator *allocator) : mAllocator(allocator)
    {
        mAllocator->push();
        SetGlobalPoolAllocator(mAllocator);
    }
    ~TScopedPoolAllocator()
    {
        SetGlobalPoolAllocator(nullptr);
        mAllocator->pop();
    }

  private:
    TPoolAllocator *mAllocator;
};

class TScopedSymbolTableLevel
{
  public:
    TScopedSymbolTableLevel(TSymbolTable *table) : mTable(table)
    {
        ASSERT(mTable->isEmpty());
        mTable->push();
    }
    ~TScopedSymbolTableLevel()
    {
        while (!mTable->isEmpty())
            mTable->pop();
    }

  private:
    TSymbolTable *mTable;
};

int MapSpecToShaderVersion(ShShaderSpec spec)
{
    switch (spec)
    {
        case SH_GLES2_SPEC:
        case SH_WEBGL_SPEC:
            return 100;
        case SH_GLES3_SPEC:
        case SH_WEBGL2_SPEC:
            return 300;
        case SH_GLES3_1_SPEC:
        case SH_WEBGL3_SPEC:
            return 310;
        default:
            UNREACHABLE();
            return 0;
    }
}

bool ValidateFragColorAndFragData(GLenum shaderType,
                                  int shaderVersion,
                                  const TSymbolTable &symbolTable,
                                  TDiagnostics *diagnostics)
{
    if (shaderVersion > 100 || shaderType != GL_FRAGMENT_SHADER)
    {
        return true;
    }

    bool usesFragColor = false;
    bool usesFragData  = false;
    // This validation is a bit stricter than the spec - it's only an error to write to
    // both FragData and FragColor. But because it's better not to have reads from undefined
    // variables, we always return an error if they are both referenced, rather than only if they
    // are written.
    if (symbolTable.isStaticallyUsed(*BuiltInVariable::gl_FragColor()) ||
        symbolTable.isStaticallyUsed(*BuiltInVariable::gl_SecondaryFragColorEXT()))
    {
        usesFragColor = true;
    }
    // Extension variables may not always be initialized (saves some time at symbol table init).
    bool secondaryFragDataUsed =
        symbolTable.gl_SecondaryFragDataEXT() != nullptr &&
        symbolTable.isStaticallyUsed(*symbolTable.gl_SecondaryFragDataEXT());
    if (symbolTable.isStaticallyUsed(*symbolTable.gl_FragData()) || secondaryFragDataUsed)
    {
        usesFragData = true;
    }
    if (usesFragColor && usesFragData)
    {
        const char *errorMessage = "cannot use both gl_FragData and gl_FragColor";
        if (symbolTable.isStaticallyUsed(*BuiltInVariable::gl_SecondaryFragColorEXT()) ||
            secondaryFragDataUsed)
        {
            errorMessage =
                "cannot use both output variable sets (gl_FragData, gl_SecondaryFragDataEXT)"
                " and (gl_FragColor, gl_SecondaryFragColorEXT)";
        }
        diagnostics->globalError(errorMessage);
        return false;
    }
    return true;
}

}  // namespace

TShHandleBase::TShHandleBase()
{
    allocator.push();
    SetGlobalPoolAllocator(&allocator);
}

TShHandleBase::~TShHandleBase()
{
    SetGlobalPoolAllocator(nullptr);
    allocator.popAll();
}

TCompiler::TCompiler(sh::GLenum type, ShShaderSpec spec, ShShaderOutput output)
    : variablesCollected(false),
      mGLPositionInitialized(false),
      shaderType(type),
      shaderSpec(spec),
      outputType(output),
      maxUniformVectors(0),
      maxExpressionComplexity(0),
      maxCallStackDepth(0),
      maxFunctionParameters(0),
      fragmentPrecisionHigh(false),
      clampingStrategy(SH_CLAMP_WITH_CLAMP_INTRINSIC),
      builtInFunctionEmulator(),
      mDiagnostics(infoSink.info),
      mSourcePath(nullptr),
      mComputeShaderLocalSizeDeclared(false),
      mComputeShaderLocalSize(1),
      mGeometryShaderMaxVertices(-1),
      mGeometryShaderInvocations(0),
      mGeometryShaderInputPrimitiveType(EptUndefined),
      mGeometryShaderOutputPrimitiveType(EptUndefined)
{
}

TCompiler::~TCompiler()
{
}

bool TCompiler::shouldRunLoopAndIndexingValidation(ShCompileOptions compileOptions) const
{
    // If compiling an ESSL 1.00 shader for WebGL, or if its been requested through the API,
    // validate loop and indexing as well (to verify that the shader only uses minimal functionality
    // of ESSL 1.00 as in Appendix A of the spec).
    return (IsWebGLBasedSpec(shaderSpec) && shaderVersion == 100) ||
           (compileOptions & SH_VALIDATE_LOOP_INDEXING);
}

bool TCompiler::Init(const ShBuiltInResources &resources)
{
    shaderVersion = 100;

    maxUniformVectors = GetMaxUniformVectorsForShaderType(shaderType, resources);

    maxExpressionComplexity = resources.MaxExpressionComplexity;
    maxCallStackDepth       = resources.MaxCallStackDepth;
    maxFunctionParameters   = resources.MaxFunctionParameters;

    SetGlobalPoolAllocator(&allocator);

    // Generate built-in symbol table.
    if (!InitBuiltInSymbolTable(resources))
        return false;
    InitExtensionBehavior(resources, extensionBehavior);
    fragmentPrecisionHigh = resources.FragmentPrecisionHigh == 1;

    arrayBoundsClamper.SetClampingStrategy(resources.ArrayIndexClampingStrategy);
    clampingStrategy = resources.ArrayIndexClampingStrategy;

    hashFunction = resources.HashFunction;

    return true;
}

TIntermBlock *TCompiler::compileTreeForTesting(const char *const shaderStrings[],
                                               size_t numStrings,
                                               ShCompileOptions compileOptions)
{
    return compileTreeImpl(shaderStrings, numStrings, compileOptions);
}

TIntermBlock *TCompiler::compileTreeImpl(const char *const shaderStrings[],
                                         size_t numStrings,
                                         const ShCompileOptions compileOptions)
{
    clearResults();

    ASSERT(numStrings > 0);
    ASSERT(GetGlobalPoolAllocator());

    // Reset the extension behavior for each compilation unit.
    ResetExtensionBehavior(extensionBehavior);

    // First string is path of source file if flag is set. The actual source follows.
    size_t firstSource = 0;
    if (compileOptions & SH_SOURCE_PATH)
    {
        mSourcePath = shaderStrings[0];
        ++firstSource;
    }

    TParseContext parseContext(symbolTable, extensionBehavior, shaderType, shaderSpec,
                               compileOptions, true, &mDiagnostics, getResources());

    parseContext.setFragmentPrecisionHighOnESSL1(fragmentPrecisionHigh);

    // We preserve symbols at the built-in level from compile-to-compile.
    // Start pushing the user-defined symbols at global level.
    TScopedSymbolTableLevel globalLevel(&symbolTable);
    ASSERT(symbolTable.atGlobalLevel());

    // Parse shader.
    if (PaParseStrings(numStrings - firstSource, &shaderStrings[firstSource], nullptr,
                       &parseContext) != 0)
    {
        return nullptr;
    }

    if (parseContext.getTreeRoot() == nullptr)
    {
        return nullptr;
    }

    setASTMetadata(parseContext);

    if (!checkShaderVersion(&parseContext))
    {
        return nullptr;
    }

    TIntermBlock *root = parseContext.getTreeRoot();
    if (!checkAndSimplifyAST(root, parseContext, compileOptions))
    {
        return nullptr;
    }

    return root;
}

bool TCompiler::checkShaderVersion(TParseContext *parseContext)
{
    if (MapSpecToShaderVersion(shaderSpec) < shaderVersion)
    {
        mDiagnostics.globalError("unsupported shader version");
        return false;
    }

    ASSERT(parseContext);
    switch (shaderType)
    {
        case GL_COMPUTE_SHADER:
            if (shaderVersion < 310)
            {
                mDiagnostics.globalError("Compute shader is not supported in this shader version.");
                return false;
            }
            break;

        case GL_GEOMETRY_SHADER_EXT:
            if (shaderVersion < 310)
            {
                mDiagnostics.globalError(
                    "Geometry shader is not supported in this shader version.");
                return false;
            }
            else
            {
                ASSERT(shaderVersion == 310);
                if (!parseContext->checkCanUseExtension(sh::TSourceLoc(),
                                                        TExtension::EXT_geometry_shader))
                {
                    return false;
                }
            }
            break;

        default:
            break;
    }

    return true;
}

void TCompiler::setASTMetadata(const TParseContext &parseContext)
{
    shaderVersion = parseContext.getShaderVersion();

    mPragma = parseContext.pragma();
    symbolTable.setGlobalInvariant(mPragma.stdgl.invariantAll);

    mComputeShaderLocalSizeDeclared = parseContext.isComputeShaderLocalSizeDeclared();
    mComputeShaderLocalSize         = parseContext.getComputeShaderLocalSize();

    mNumViews = parseContext.getNumViews();

    // Highp might have been auto-enabled based on shader version
    fragmentPrecisionHigh = parseContext.getFragmentPrecisionHigh();

    if (shaderType == GL_GEOMETRY_SHADER_EXT)
    {
        mGeometryShaderInputPrimitiveType  = parseContext.getGeometryShaderInputPrimitiveType();
        mGeometryShaderOutputPrimitiveType = parseContext.getGeometryShaderOutputPrimitiveType();
        mGeometryShaderMaxVertices         = parseContext.getGeometryShaderMaxVertices();
        mGeometryShaderInvocations         = parseContext.getGeometryShaderInvocations();
    }
}

bool TCompiler::checkAndSimplifyAST(TIntermBlock *root,
                                    const TParseContext &parseContext,
                                    ShCompileOptions compileOptions)
{
    // Disallow expressions deemed too complex.
    if ((compileOptions & SH_LIMIT_EXPRESSION_COMPLEXITY) && !limitExpressionComplexity(root))
    {
        return false;
    }

    if (shouldRunLoopAndIndexingValidation(compileOptions) &&
        !ValidateLimitations(root, shaderType, &symbolTable, &mDiagnostics))
    {
        return false;
    }

    if (!ValidateFragColorAndFragData(shaderType, shaderVersion, symbolTable, &mDiagnostics))
    {
        return false;
    }

    // Fold expressions that could not be folded before validation that was done as a part of
    // parsing.
    FoldExpressions(root, &mDiagnostics);
    // Folding should only be able to generate warnings.
    ASSERT(mDiagnostics.numErrors() == 0);

    // We prune no-ops to work around driver bugs and to keep AST processing and output simple.
    // The following kinds of no-ops are pruned:
    //   1. Empty declarations "int;".
    //   2. Literal statements: "1.0;". The ESSL output doesn't define a default precision
    //      for float, so float literal statements would end up with no precision which is
    //      invalid ESSL.
    // After this empty declarations are not allowed in the AST.
    PruneNoOps(root, &symbolTable);

    // Create the function DAG and check there is no recursion
    if (!initCallDag(root))
    {
        return false;
    }

    if ((compileOptions & SH_LIMIT_CALL_STACK_DEPTH) && !checkCallDepth())
    {
        return false;
    }

    // Checks which functions are used and if "main" exists
    functionMetadata.clear();
    functionMetadata.resize(mCallDag.size());
    if (!tagUsedFunctions())
    {
        return false;
    }

    if (!(compileOptions & SH_DONT_PRUNE_UNUSED_FUNCTIONS))
    {
        pruneUnusedFunctions(root);
    }

    if (shaderVersion >= 310 && !ValidateVaryingLocations(root, &mDiagnostics, shaderType))
    {
        return false;
    }

    if (shaderVersion >= 300 && shaderType == GL_FRAGMENT_SHADER &&
        !ValidateOutputs(root, getExtensionBehavior(), compileResources.MaxDrawBuffers,
                         &mDiagnostics))
    {
        return false;
    }

    // Fail compilation if precision emulation not supported.
    if (getResources().WEBGL_debug_shader_precision && getPragma().debugShaderPrecision &&
        !EmulatePrecision::SupportedInLanguage(outputType))
    {
        mDiagnostics.globalError("Precision emulation not supported for this output type.");
        return false;
    }

    // Clamping uniform array bounds needs to happen after validateLimitations pass.
    if (compileOptions & SH_CLAMP_INDIRECT_ARRAY_BOUNDS)
    {
        arrayBoundsClamper.MarkIndirectArrayBoundsForClamping(root);
    }

    if ((compileOptions & SH_INITIALIZE_BUILTINS_FOR_INSTANCED_MULTIVIEW) &&
        parseContext.isExtensionEnabled(TExtension::OVR_multiview) &&
        getShaderType() != GL_COMPUTE_SHADER)
    {
        DeclareAndInitBuiltinsForInstancedMultiview(root, mNumViews, shaderType, compileOptions,
                                                    outputType, &symbolTable);
    }

    // This pass might emit short circuits so keep it before the short circuit unfolding
    if (compileOptions & SH_REWRITE_DO_WHILE_LOOPS)
        RewriteDoWhile(root, &symbolTable);

    if (compileOptions & SH_ADD_AND_TRUE_TO_LOOP_CONDITION)
        AddAndTrueToLoopCondition(root);

    if (compileOptions & SH_UNFOLD_SHORT_CIRCUIT)
    {
        UnfoldShortCircuitAST(root);
    }

    if (compileOptions & SH_REMOVE_POW_WITH_CONSTANT_EXPONENT)
    {
        RemovePow(root, &symbolTable);
    }

    if (compileOptions & SH_REGENERATE_STRUCT_NAMES)
    {
        RegenerateStructNames gen(&symbolTable);
        root->traverse(&gen);
    }

    if (shaderType == GL_FRAGMENT_SHADER && shaderVersion == 100 &&
        compileResources.EXT_draw_buffers && compileResources.MaxDrawBuffers > 1 &&
        IsExtensionEnabled(extensionBehavior, TExtension::EXT_draw_buffers))
    {
        EmulateGLFragColorBroadcast(root, compileResources.MaxDrawBuffers, &outputVariables,
                                    &symbolTable, shaderVersion);
    }

    int simplifyScalarized = (compileOptions & SH_SCALARIZE_VEC_AND_MAT_CONSTRUCTOR_ARGS)
                                 ? IntermNodePatternMatcher::kScalarizedVecOrMatConstructor
                                 : 0;

    // Split multi declarations and remove calls to array length().
    // Note that SimplifyLoopConditions needs to be run before any other AST transformations
    // that may need to generate new statements from loop conditions or loop expressions.
    SimplifyLoopConditions(root,
                           IntermNodePatternMatcher::kMultiDeclaration |
                               IntermNodePatternMatcher::kArrayLengthMethod | simplifyScalarized,
                           &getSymbolTable());

    // Note that separate declarations need to be run before other AST transformations that
    // generate new statements from expressions.
    SeparateDeclarations(root);

    SplitSequenceOperator(root, IntermNodePatternMatcher::kArrayLengthMethod | simplifyScalarized,
                          &getSymbolTable());

    RemoveArrayLengthMethod(root);

    RemoveUnreferencedVariables(root, &symbolTable);

    // In case the last case inside a switch statement is a certain type of no-op, GLSL compilers in
    // drivers may not accept it. In this case we clean up the dead code from the end of switch
    // statements. This is also required because PruneNoOps or RemoveUnreferencedVariables may have
    // left switch statements that only contained an empty declaration inside the final case in an
    // invalid state. Relies on that PruneNoOps and RemoveUnreferencedVariables have already been
    // run.
    PruneEmptyCases(root);

    // Built-in function emulation needs to happen after validateLimitations pass.
    // TODO(jmadill): Remove global pool allocator.
    GetGlobalPoolAllocator()->lock();
    initBuiltInFunctionEmulator(&builtInFunctionEmulator, compileOptions);
    GetGlobalPoolAllocator()->unlock();
    builtInFunctionEmulator.markBuiltInFunctionsForEmulation(root);

    if (compileOptions & SH_SCALARIZE_VEC_AND_MAT_CONSTRUCTOR_ARGS)
    {
        ScalarizeVecAndMatConstructorArgs(root, shaderType, fragmentPrecisionHigh, &symbolTable);
    }

    if (shouldCollectVariables(compileOptions))
    {
        ASSERT(!variablesCollected);
        CollectVariables(root, &attributes, &outputVariables, &uniforms, &inputVaryings,
                         &outputVaryings, &uniformBlocks, &shaderStorageBlocks, &inBlocks,
                         hashFunction, &symbolTable, shaderType, extensionBehavior);
        collectInterfaceBlocks();
        variablesCollected = true;
        if (compileOptions & SH_USE_UNUSED_STANDARD_SHARED_BLOCKS)
        {
            useAllMembersInUnusedStandardAndSharedBlocks(root);
        }
        if (compileOptions & SH_ENFORCE_PACKING_RESTRICTIONS)
        {
            // Returns true if, after applying the packing rules in the GLSL ES 1.00.17 spec
            // Appendix A, section 7, the shader does not use too many uniforms.
            if (!CheckVariablesInPackingLimits(maxUniformVectors, uniforms))
            {
                mDiagnostics.globalError("too many uniforms");
                return false;
            }
        }
        if (compileOptions & SH_INIT_OUTPUT_VARIABLES)
        {
            initializeOutputVariables(root);
        }
    }

    // Removing invariant declarations must be done after collecting variables.
    // Otherwise, built-in invariant declarations don't apply.
    if (RemoveInvariant(shaderType, shaderVersion, outputType, compileOptions))
    {
        RemoveInvariantDeclaration(root);
    }

    // gl_Position is always written in compatibility output mode.
    // It may have been already initialized among other output variables, in that case we don't
    // need to initialize it twice.
    if (shaderType == GL_VERTEX_SHADER && !mGLPositionInitialized &&
        ((compileOptions & SH_INIT_GL_POSITION) || (outputType == SH_GLSL_COMPATIBILITY_OUTPUT)))
    {
        initializeGLPosition(root);
        mGLPositionInitialized = true;
    }

    // DeferGlobalInitializers needs to be run before other AST transformations that generate new
    // statements from expressions. But it's fine to run DeferGlobalInitializers after the above
    // SplitSequenceOperator and RemoveArrayLengthMethod since they only have an effect on the AST
    // on ESSL >= 3.00, and the initializers that need to be deferred can only exist in ESSL < 3.00.
    bool initializeLocalsAndGlobals =
        (compileOptions & SH_INITIALIZE_UNINITIALIZED_LOCALS) && !IsOutputHLSL(getOutputType());
    bool canUseLoopsToInitialize = !(compileOptions & SH_DONT_USE_LOOPS_TO_INITIALIZE_VARIABLES);
    bool highPrecisionSupported =
        shaderType != GL_FRAGMENT_SHADER || compileResources.FragmentPrecisionHigh;
    DeferGlobalInitializers(root, initializeLocalsAndGlobals, canUseLoopsToInitialize,
                            highPrecisionSupported, &symbolTable);

    if (initializeLocalsAndGlobals)
    {
        // Initialize uninitialized local variables.
        // In some cases initializing can generate extra statements in the parent block, such as
        // when initializing nameless structs or initializing arrays in ESSL 1.00. In that case
        // we need to first simplify loop conditions. We've already separated declarations
        // earlier, which is also required. If we don't follow the Appendix A limitations, loop
        // init statements can declare arrays or nameless structs and have multiple
        // declarations.

        if (!shouldRunLoopAndIndexingValidation(compileOptions))
        {
            SimplifyLoopConditions(root,
                                   IntermNodePatternMatcher::kArrayDeclaration |
                                       IntermNodePatternMatcher::kNamelessStructDeclaration,
                                   &getSymbolTable());
        }

        InitializeUninitializedLocals(root, getShaderVersion(), canUseLoopsToInitialize,
                                      highPrecisionSupported, &getSymbolTable());
    }

    if (getShaderType() == GL_VERTEX_SHADER && (compileOptions & SH_CLAMP_POINT_SIZE))
    {
        ClampPointSize(root, compileResources.MaxPointSize, &getSymbolTable());
    }

    if (getShaderType() == GL_FRAGMENT_SHADER && (compileOptions & SH_CLAMP_FRAG_DEPTH))
    {
        ClampFragDepth(root, &getSymbolTable());
    }

    if (compileOptions & SH_REWRITE_REPEATED_ASSIGN_TO_SWIZZLED)
    {
        sh::RewriteRepeatedAssignToSwizzled(root);
    }

    if (compileOptions & SH_REWRITE_VECTOR_SCALAR_ARITHMETIC)
    {
        VectorizeVectorScalarArithmetic(root, &getSymbolTable());
    }

    return true;
}

bool TCompiler::compile(const char *const shaderStrings[],
                        size_t numStrings,
                        ShCompileOptions compileOptionsIn)
{
#if defined(ANGLE_ENABLE_FUZZER_CORPUS_OUTPUT)
    DumpFuzzerCase(shaderStrings, numStrings, shaderType, shaderSpec, outputType, compileOptionsIn);
#endif  // defined(ANGLE_ENABLE_FUZZER_CORPUS_OUTPUT)

    if (numStrings == 0)
        return true;

    ShCompileOptions compileOptions = compileOptionsIn;

    // Apply key workarounds.
    if (shouldFlattenPragmaStdglInvariantAll())
    {
        // This should be harmless to do in all cases, but for the moment, do it only conditionally.
        compileOptions |= SH_FLATTEN_PRAGMA_STDGL_INVARIANT_ALL;
    }

    TScopedPoolAllocator scopedAlloc(&allocator);
    TIntermBlock *root = compileTreeImpl(shaderStrings, numStrings, compileOptions);

    if (root)
    {
        if (compileOptions & SH_INTERMEDIATE_TREE)
            OutputTree(root, infoSink.info);

        if (compileOptions & SH_OBJECT_CODE)
        {
            PerformanceDiagnostics perfDiagnostics(&mDiagnostics);
            translate(root, compileOptions, &perfDiagnostics);
        }

        // The IntermNode tree doesn't need to be deleted here, since the
        // memory will be freed in a big chunk by the PoolAllocator.
        return true;
    }
    return false;
}

bool TCompiler::InitBuiltInSymbolTable(const ShBuiltInResources &resources)
{
    if (resources.MaxDrawBuffers < 1)
    {
        return false;
    }
    if (resources.EXT_blend_func_extended && resources.MaxDualSourceDrawBuffers < 1)
    {
        return false;
    }

    compileResources = resources;
    setResourceString();

    symbolTable.initializeBuiltIns(shaderType, shaderSpec, resources);

    return true;
}

void TCompiler::setResourceString()
{
    std::ostringstream strstream;

    // clang-format off
    strstream << ":MaxVertexAttribs:" << compileResources.MaxVertexAttribs
        << ":MaxVertexUniformVectors:" << compileResources.MaxVertexUniformVectors
        << ":MaxVaryingVectors:" << compileResources.MaxVaryingVectors
        << ":MaxVertexTextureImageUnits:" << compileResources.MaxVertexTextureImageUnits
        << ":MaxCombinedTextureImageUnits:" << compileResources.MaxCombinedTextureImageUnits
        << ":MaxTextureImageUnits:" << compileResources.MaxTextureImageUnits
        << ":MaxFragmentUniformVectors:" << compileResources.MaxFragmentUniformVectors
        << ":MaxDrawBuffers:" << compileResources.MaxDrawBuffers
        << ":OES_standard_derivatives:" << compileResources.OES_standard_derivatives
        << ":OES_EGL_image_external:" << compileResources.OES_EGL_image_external
        << ":OES_EGL_image_external_essl3:" << compileResources.OES_EGL_image_external_essl3
        << ":NV_EGL_stream_consumer_external:" << compileResources.NV_EGL_stream_consumer_external
        << ":ARB_texture_rectangle:" << compileResources.ARB_texture_rectangle
        << ":EXT_draw_buffers:" << compileResources.EXT_draw_buffers
        << ":FragmentPrecisionHigh:" << compileResources.FragmentPrecisionHigh
        << ":MaxExpressionComplexity:" << compileResources.MaxExpressionComplexity
        << ":MaxCallStackDepth:" << compileResources.MaxCallStackDepth
        << ":MaxFunctionParameters:" << compileResources.MaxFunctionParameters
        << ":EXT_blend_func_extended:" << compileResources.EXT_blend_func_extended
        << ":EXT_frag_depth:" << compileResources.EXT_frag_depth
        << ":EXT_shader_texture_lod:" << compileResources.EXT_shader_texture_lod
        << ":EXT_shader_framebuffer_fetch:" << compileResources.EXT_shader_framebuffer_fetch
        << ":NV_shader_framebuffer_fetch:" << compileResources.NV_shader_framebuffer_fetch
        << ":ARM_shader_framebuffer_fetch:" << compileResources.ARM_shader_framebuffer_fetch
        << ":OVR_multiview:" << compileResources.OVR_multiview
        << ":EXT_YUV_target:" << compileResources.EXT_YUV_target
        << ":EXT_geometry_shader:" << compileResources.EXT_geometry_shader
        << ":MaxVertexOutputVectors:" << compileResources.MaxVertexOutputVectors
        << ":MaxFragmentInputVectors:" << compileResources.MaxFragmentInputVectors
        << ":MinProgramTexelOffset:" << compileResources.MinProgramTexelOffset
        << ":MaxProgramTexelOffset:" << compileResources.MaxProgramTexelOffset
        << ":MaxDualSourceDrawBuffers:" << compileResources.MaxDualSourceDrawBuffers
        << ":MaxViewsOVR:" << compileResources.MaxViewsOVR
        << ":NV_draw_buffers:" << compileResources.NV_draw_buffers
        << ":WEBGL_debug_shader_precision:" << compileResources.WEBGL_debug_shader_precision
        << ":MinProgramTextureGatherOffset:" << compileResources.MinProgramTextureGatherOffset
        << ":MaxProgramTextureGatherOffset:" << compileResources.MaxProgramTextureGatherOffset
        << ":MaxImageUnits:" << compileResources.MaxImageUnits
        << ":MaxVertexImageUniforms:" << compileResources.MaxVertexImageUniforms
        << ":MaxFragmentImageUniforms:" << compileResources.MaxFragmentImageUniforms
        << ":MaxComputeImageUniforms:" << compileResources.MaxComputeImageUniforms
        << ":MaxCombinedImageUniforms:" << compileResources.MaxCombinedImageUniforms
        << ":MaxCombinedShaderOutputResources:" << compileResources.MaxCombinedShaderOutputResources
        << ":MaxComputeWorkGroupCountX:" << compileResources.MaxComputeWorkGroupCount[0]
        << ":MaxComputeWorkGroupCountY:" << compileResources.MaxComputeWorkGroupCount[1]
        << ":MaxComputeWorkGroupCountZ:" << compileResources.MaxComputeWorkGroupCount[2]
        << ":MaxComputeWorkGroupSizeX:" << compileResources.MaxComputeWorkGroupSize[0]
        << ":MaxComputeWorkGroupSizeY:" << compileResources.MaxComputeWorkGroupSize[1]
        << ":MaxComputeWorkGroupSizeZ:" << compileResources.MaxComputeWorkGroupSize[2]
        << ":MaxComputeUniformComponents:" << compileResources.MaxComputeUniformComponents
        << ":MaxComputeTextureImageUnits:" << compileResources.MaxComputeTextureImageUnits
        << ":MaxComputeAtomicCounters:" << compileResources.MaxComputeAtomicCounters
        << ":MaxComputeAtomicCounterBuffers:" << compileResources.MaxComputeAtomicCounterBuffers
        << ":MaxVertexAtomicCounters:" << compileResources.MaxVertexAtomicCounters
        << ":MaxFragmentAtomicCounters:" << compileResources.MaxFragmentAtomicCounters
        << ":MaxCombinedAtomicCounters:" << compileResources.MaxCombinedAtomicCounters
        << ":MaxAtomicCounterBindings:" << compileResources.MaxAtomicCounterBindings
        << ":MaxVertexAtomicCounterBuffers:" << compileResources.MaxVertexAtomicCounterBuffers
        << ":MaxFragmentAtomicCounterBuffers:" << compileResources.MaxFragmentAtomicCounterBuffers
        << ":MaxCombinedAtomicCounterBuffers:" << compileResources.MaxCombinedAtomicCounterBuffers
        << ":MaxAtomicCounterBufferSize:" << compileResources.MaxAtomicCounterBufferSize
        << ":MaxGeometryUniformComponents:" << compileResources.MaxGeometryUniformComponents
        << ":MaxGeometryUniformBlocks:" << compileResources.MaxGeometryUniformBlocks
        << ":MaxGeometryInputComponents:" << compileResources.MaxGeometryInputComponents
        << ":MaxGeometryOutputComponents:" << compileResources.MaxGeometryOutputComponents
        << ":MaxGeometryOutputVertices:" << compileResources.MaxGeometryOutputVertices
        << ":MaxGeometryTotalOutputComponents:" << compileResources.MaxGeometryTotalOutputComponents
        << ":MaxGeometryTextureImageUnits:" << compileResources.MaxGeometryTextureImageUnits
        << ":MaxGeometryAtomicCounterBuffers:" << compileResources.MaxGeometryAtomicCounterBuffers
        << ":MaxGeometryAtomicCounters:" << compileResources.MaxGeometryAtomicCounters
        << ":MaxGeometryShaderStorageBlocks:" << compileResources.MaxGeometryShaderStorageBlocks
        << ":MaxGeometryShaderInvocations:" << compileResources.MaxGeometryShaderInvocations
        << ":MaxGeometryImageUniforms:" << compileResources.MaxGeometryImageUniforms;
    // clang-format on

    builtInResourcesString = strstream.str();
}

void TCompiler::collectInterfaceBlocks()
{
    ASSERT(interfaceBlocks.empty());
    interfaceBlocks.reserve(uniformBlocks.size() + shaderStorageBlocks.size() + inBlocks.size());
    interfaceBlocks.insert(interfaceBlocks.end(), uniformBlocks.begin(), uniformBlocks.end());
    interfaceBlocks.insert(interfaceBlocks.end(), shaderStorageBlocks.begin(),
                           shaderStorageBlocks.end());
    interfaceBlocks.insert(interfaceBlocks.end(), inBlocks.begin(), inBlocks.end());
}

void TCompiler::clearResults()
{
    arrayBoundsClamper.Cleanup();
    infoSink.info.erase();
    infoSink.obj.erase();
    infoSink.debug.erase();
    mDiagnostics.resetErrorCount();

    attributes.clear();
    outputVariables.clear();
    uniforms.clear();
    inputVaryings.clear();
    outputVaryings.clear();
    interfaceBlocks.clear();
    uniformBlocks.clear();
    shaderStorageBlocks.clear();
    inBlocks.clear();
    variablesCollected = false;
    mGLPositionInitialized = false;

    mNumViews = -1;

    mGeometryShaderInputPrimitiveType  = EptUndefined;
    mGeometryShaderOutputPrimitiveType = EptUndefined;
    mGeometryShaderInvocations         = 0;
    mGeometryShaderMaxVertices         = -1;

    builtInFunctionEmulator.cleanup();

    nameMap.clear();

    mSourcePath     = nullptr;

    symbolTable.clearCompilationResults();
}

bool TCompiler::initCallDag(TIntermNode *root)
{
    mCallDag.clear();

    switch (mCallDag.init(root, &mDiagnostics))
    {
        case CallDAG::INITDAG_SUCCESS:
            return true;
        case CallDAG::INITDAG_RECURSION:
        case CallDAG::INITDAG_UNDEFINED:
            // Error message has already been written out.
            ASSERT(mDiagnostics.numErrors() > 0);
            return false;
    }

    UNREACHABLE();
    return true;
}

bool TCompiler::checkCallDepth()
{
    std::vector<int> depths(mCallDag.size());

    for (size_t i = 0; i < mCallDag.size(); i++)
    {
        int depth    = 0;
        auto &record = mCallDag.getRecordFromIndex(i);

        for (auto &calleeIndex : record.callees)
        {
            depth = std::max(depth, depths[calleeIndex] + 1);
        }

        depths[i] = depth;

        if (depth >= maxCallStackDepth)
        {
            // Trace back the function chain to have a meaningful info log.
            std::stringstream errorStream;
            errorStream << "Call stack too deep (larger than " << maxCallStackDepth
                        << ") with the following call chain: "
                        << record.node->getFunction()->name();

            int currentFunction = static_cast<int>(i);
            int currentDepth    = depth;

            while (currentFunction != -1)
            {
                errorStream
                    << " -> "
                    << mCallDag.getRecordFromIndex(currentFunction).node->getFunction()->name();

                int nextFunction = -1;
                for (auto &calleeIndex : mCallDag.getRecordFromIndex(currentFunction).callees)
                {
                    if (depths[calleeIndex] == currentDepth - 1)
                    {
                        currentDepth--;
                        nextFunction = calleeIndex;
                    }
                }

                currentFunction = nextFunction;
            }

            std::string errorStr = errorStream.str();
            mDiagnostics.globalError(errorStr.c_str());

            return false;
        }
    }

    return true;
}

bool TCompiler::tagUsedFunctions()
{
    // Search from main, starting from the end of the DAG as it usually is the root.
    for (size_t i = mCallDag.size(); i-- > 0;)
    {
        if (mCallDag.getRecordFromIndex(i).node->getFunction()->isMain())
        {
            internalTagUsedFunction(i);
            return true;
        }
    }

    mDiagnostics.globalError("Missing main()");
    return false;
}

void TCompiler::internalTagUsedFunction(size_t index)
{
    if (functionMetadata[index].used)
    {
        return;
    }

    functionMetadata[index].used = true;

    for (int calleeIndex : mCallDag.getRecordFromIndex(index).callees)
    {
        internalTagUsedFunction(calleeIndex);
    }
}

// A predicate for the stl that returns if a top-level node is unused
class TCompiler::UnusedPredicate
{
  public:
    UnusedPredicate(const CallDAG *callDag, const std::vector<FunctionMetadata> *metadatas)
        : mCallDag(callDag), mMetadatas(metadatas)
    {
    }

    bool operator()(TIntermNode *node)
    {
        const TIntermFunctionPrototype *asFunctionPrototype   = node->getAsFunctionPrototypeNode();
        const TIntermFunctionDefinition *asFunctionDefinition = node->getAsFunctionDefinition();

        const TFunction *func = nullptr;

        if (asFunctionDefinition)
        {
            func = asFunctionDefinition->getFunction();
        }
        else if (asFunctionPrototype)
        {
            func = asFunctionPrototype->getFunction();
        }
        if (func == nullptr)
        {
            return false;
        }

        size_t callDagIndex = mCallDag->findIndex(func->uniqueId());
        if (callDagIndex == CallDAG::InvalidIndex)
        {
            // This happens only for unimplemented prototypes which are thus unused
            ASSERT(asFunctionPrototype);
            return true;
        }

        ASSERT(callDagIndex < mMetadatas->size());
        return !(*mMetadatas)[callDagIndex].used;
    }

  private:
    const CallDAG *mCallDag;
    const std::vector<FunctionMetadata> *mMetadatas;
};

void TCompiler::pruneUnusedFunctions(TIntermBlock *root)
{
    UnusedPredicate isUnused(&mCallDag, &functionMetadata);
    TIntermSequence *sequence = root->getSequence();

    if (!sequence->empty())
    {
        sequence->erase(std::remove_if(sequence->begin(), sequence->end(), isUnused),
                        sequence->end());
    }
}

bool TCompiler::limitExpressionComplexity(TIntermBlock *root)
{
    if (!IsASTDepthBelowLimit(root, maxExpressionComplexity))
    {
        mDiagnostics.globalError("Expression too complex.");
        return false;
    }

    if (!ValidateMaxParameters(root, maxFunctionParameters))
    {
        mDiagnostics.globalError("Function has too many parameters.");
        return false;
    }

    return true;
}

bool TCompiler::shouldCollectVariables(ShCompileOptions compileOptions)
{
    return (compileOptions & SH_VARIABLES) != 0;
}

bool TCompiler::wereVariablesCollected() const
{
    return variablesCollected;
}

void TCompiler::initializeGLPosition(TIntermBlock *root)
{
    InitVariableList list;
    sh::ShaderVariable var(GL_FLOAT_VEC4);
    var.name = "gl_Position";
    list.push_back(var);
    InitializeVariables(root, list, &symbolTable, shaderVersion, extensionBehavior, false, false);
}

void TCompiler::useAllMembersInUnusedStandardAndSharedBlocks(TIntermBlock *root)
{
    sh::InterfaceBlockList list;

    for (auto block : uniformBlocks)
    {
        if (!block.staticUse &&
            (block.layout == sh::BLOCKLAYOUT_STD140 || block.layout == sh::BLOCKLAYOUT_SHARED))
        {
            list.push_back(block);
        }
    }

    sh::UseInterfaceBlockFields(root, list, symbolTable);
}

void TCompiler::initializeOutputVariables(TIntermBlock *root)
{
    InitVariableList list;
    if (shaderType == GL_VERTEX_SHADER || shaderType == GL_GEOMETRY_SHADER_EXT)
    {
        for (auto var : outputVaryings)
        {
            list.push_back(var);
            if (var.name == "gl_Position")
            {
                ASSERT(!mGLPositionInitialized);
                mGLPositionInitialized = true;
            }
        }
    }
    else
    {
        ASSERT(shaderType == GL_FRAGMENT_SHADER);
        for (auto var : outputVariables)
        {
            list.push_back(var);
        }
    }
    InitializeVariables(root, list, &symbolTable, shaderVersion, extensionBehavior, false, false);
}

const TExtensionBehavior &TCompiler::getExtensionBehavior() const
{
    return extensionBehavior;
}

const char *TCompiler::getSourcePath() const
{
    return mSourcePath;
}

const ShBuiltInResources &TCompiler::getResources() const
{
    return compileResources;
}

const ArrayBoundsClamper &TCompiler::getArrayBoundsClamper() const
{
    return arrayBoundsClamper;
}

ShArrayIndexClampingStrategy TCompiler::getArrayIndexClampingStrategy() const
{
    return clampingStrategy;
}

const BuiltInFunctionEmulator &TCompiler::getBuiltInFunctionEmulator() const
{
    return builtInFunctionEmulator;
}

void TCompiler::writePragma(ShCompileOptions compileOptions)
{
    if (!(compileOptions & SH_FLATTEN_PRAGMA_STDGL_INVARIANT_ALL))
    {
        TInfoSinkBase &sink = infoSink.obj;
        if (mPragma.stdgl.invariantAll)
            sink << "#pragma STDGL invariant(all)\n";
    }
}

bool TCompiler::isVaryingDefined(const char *varyingName)
{
    ASSERT(variablesCollected);
    for (size_t ii = 0; ii < inputVaryings.size(); ++ii)
    {
        if (inputVaryings[ii].name == varyingName)
        {
            return true;
        }
    }
    for (size_t ii = 0; ii < outputVaryings.size(); ++ii)
    {
        if (outputVaryings[ii].name == varyingName)
        {
            return true;
        }
    }

    return false;
}

}  // namespace sh
