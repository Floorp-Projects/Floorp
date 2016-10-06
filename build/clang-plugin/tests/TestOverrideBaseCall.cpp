#define MOZ_REQUIRED_BASE_METHOD __attribute__((annotate("moz_required_base_method")))

class Base {
public:
  virtual void fo() MOZ_REQUIRED_BASE_METHOD {
  }

  virtual int foRet() MOZ_REQUIRED_BASE_METHOD {
    return 0;
  }
};

class BaseOne : public Base {
public:
  virtual void fo() MOZ_REQUIRED_BASE_METHOD {
    Base::fo();
  }
};

class BaseSecond : public Base {
public:
  virtual void fo() MOZ_REQUIRED_BASE_METHOD {
   Base::fo();
  }
};

class Deriv : public BaseOne, public BaseSecond {
public:
  void func() {
  }

  void fo() {
    func();
    BaseSecond::fo();
    BaseOne::fo();
  }
};

class DerivSimple : public Base {
public:
  void fo() { // expected-error {{Method Base::fo must be called in all overrides, but is not called in this override defined for class DerivSimple}}
  }
};

class BaseVirtualOne : public virtual Base {
};

class BaseVirtualSecond: public virtual Base {
};

class DerivVirtual : public BaseVirtualOne, public BaseVirtualSecond {
public:
  void fo() {
    Base::fo();
  }
};

class DerivIf : public Base {
public:
  void fo() {
    if (true) {
      Base::fo();
    }
  }
};

class DerivIfElse : public Base {
public:
  void fo() {
    if (true) {
      Base::fo();
    } else {
      Base::fo();
    }
  }
};

class DerivFor : public Base {
public:
  void fo() {
    for (int i = 0; i < 10; i++) {
      Base::fo();
    }
  }
};

class DerivDoWhile : public Base {
public:
  void fo() {
    do {
      Base::fo();
    } while(false);
  }
};

class DerivWhile : public Base {
public:
  void fo() {
    while (true) {
      Base::fo();
      break;
    }
  }
};

class DerivAssignment : public Base {
public:
  int foRet() {
    return foRet();
  }
};

class BaseOperator {
private:
  int value;
public:
  BaseOperator() : value(0) {
  }
  virtual BaseOperator& operator++() MOZ_REQUIRED_BASE_METHOD {
    value++;
    return *this;
  }
};

class DerivOperatorErr : public BaseOperator {
private:
  int value;
public:
  DerivOperatorErr() : value(0) {
  }
  DerivOperatorErr& operator++() { // expected-error {{Method BaseOperator::operator++ must be called in all overrides, but is not called in this override defined for class DerivOperatorErr}}
    value++;
    return *this;
  }
};

class DerivOperator : public BaseOperator {
private:
  int value;
public:
  DerivOperator() : value(0) {
  }
  DerivOperator& operator++() {
    BaseOperator::operator++();
    value++;
    return *this;
  }
};

class DerivPrime : public Base {
public:
  void fo() {
    Base::fo();
  }
};

class DerivSecondErr : public DerivPrime {
public:
  void fo() { // expected-error {{Method Base::fo must be called in all overrides, but is not called in this override defined for class DerivSecondErr}}
  }
};

class DerivSecond : public DerivPrime {
public:
  void fo() {
    Base::fo();
  }
};

class DerivSecondIndirect : public DerivPrime {
public:
  void fo() {
    DerivPrime::fo();
  }
};
