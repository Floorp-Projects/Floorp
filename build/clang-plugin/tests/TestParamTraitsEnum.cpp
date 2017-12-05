typedef enum {
  BadFirst,
  BadSecond,
  BadThird
} BadEnum;

typedef enum {
  NestedFirst,
  NestedSecond
} NestedBadEnum;

typedef enum {
  GoodFirst,
  GoodSecond,
  GoodLast
} GoodEnum;

enum RawEnum {
  RawFirst,
  RawLast
};

enum class ClassEnum {
  ClassFirst,
  ClassLast
};

template <class P> struct ParamTraits;

// Simplified EnumSerializer etc. from IPCMessageUtils.h
template <typename E, typename EnumValidator>
struct EnumSerializer {
  typedef E paramType;
};

template <typename E,
          E MinLegal,
          E HighBound>
class ContiguousEnumValidator
{};

template <typename E,
          E MinLegal,
          E HighBound>
struct ContiguousEnumSerializer
  : EnumSerializer<E,
                   ContiguousEnumValidator<E, MinLegal, HighBound>>
{};

// Typical ParamTraits implementation that should be avoided
template<>
struct ParamTraits<ClassEnum> // expected-error {{Custom ParamTraits implementation for an enum type}} expected-note {{Please use a helper class for example ContiguousEnumSerializer}}
{
  typedef ClassEnum paramType;
};

template<>
struct ParamTraits<enum RawEnum> // expected-error {{Custom ParamTraits implementation for an enum type}} expected-note {{Please use a helper class for example ContiguousEnumSerializer}}
{
  typedef enum RawEnum paramType;
};

template<>
struct ParamTraits<BadEnum> // expected-error {{Custom ParamTraits implementation for an enum type}} expected-note {{Please use a helper class for example ContiguousEnumSerializer}}
{
  typedef BadEnum paramType;
};

// Make sure the analysis catches nested typedefs
typedef NestedBadEnum NestedDefLevel1;
typedef NestedDefLevel1 NestedDefLevel2;

template<>
struct ParamTraits<NestedDefLevel2> // expected-error {{Custom ParamTraits implementation for an enum type}} expected-note {{Please use a helper class for example ContiguousEnumSerializer}}
{
  typedef NestedDefLevel2 paramType;
};

// Make sure a non enum typedef is not accidentally flagged
typedef int IntTypedef;

template<>
struct ParamTraits<IntTypedef>
{
  typedef IntTypedef paramType;
};

// Make sure ParamTraits using helper classes are not flagged
template<>
struct ParamTraits<GoodEnum>
: public ContiguousEnumSerializer<GoodEnum,
                                  GoodEnum::GoodFirst,
                                  GoodEnum::GoodLast>
{};
