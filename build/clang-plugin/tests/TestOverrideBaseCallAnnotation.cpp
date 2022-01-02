#define MOZ_REQUIRED_BASE_METHOD __attribute__((annotate("moz_required_base_method")))

class Base {
public:
  virtual void fo() MOZ_REQUIRED_BASE_METHOD {
  }
};

class BaseNonVirtual {
public:
  void fo() MOZ_REQUIRED_BASE_METHOD { // expected-error {{MOZ_REQUIRED_BASE_METHOD can be used only on virtual methods}}
  }
};

class Deriv : public BaseNonVirtual {
public:
  virtual void fo() MOZ_REQUIRED_BASE_METHOD {
  }
};

class DerivVirtual : public Base {
public:
  void fo() MOZ_REQUIRED_BASE_METHOD {
    Base::fo();
  }
};

class BaseOperator {
public:
  BaseOperator& operator++() MOZ_REQUIRED_BASE_METHOD { // expected-error {{MOZ_REQUIRED_BASE_METHOD can be used only on virtual methods}}
    return *this;
  }
};

class DerivOperator : public BaseOperator {
public:
  virtual DerivOperator& operator++() {
    return *this;
  }
};

class DerivPrimeOperator : public DerivOperator {
public:
  DerivPrimeOperator& operator++() {
    return *this;
  }
};
