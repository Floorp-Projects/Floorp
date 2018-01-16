#define MOZ_NEEDS_NO_VTABLE_TYPE __attribute__((annotate("moz_needs_no_vtable_type")))

template <class T>
struct MOZ_NEEDS_NO_VTABLE_TYPE PickyConsumer { // expected-error {{'PickyConsumer<B>' cannot be instantiated because 'B' has a VTable}} expected-error {{'PickyConsumer<E>' cannot be instantiated because 'E' has a VTable}} expected-error {{'PickyConsumer<F>' cannot be instantiated because 'F' has a VTable}} expected-error {{'PickyConsumer<G>' cannot be instantiated because 'G' has a VTable}}
  T *m;
};

template <class T>
struct MOZ_NEEDS_NO_VTABLE_TYPE PickyConsumer_A { // expected-error {{'PickyConsumer_A<B>' cannot be instantiated because 'B' has a VTable}} expected-error {{'PickyConsumer_A<E>' cannot be instantiated because 'E' has a VTable}} expected-error {{'PickyConsumer_A<F>' cannot be instantiated because 'F' has a VTable}} expected-error {{'PickyConsumer_A<G>' cannot be instantiated because 'G' has a VTable}}
  T *m;
};
template <class T>
struct PickyConsumerWrapper {
  PickyConsumer_A<T> m; // expected-note {{bad instantiation of 'PickyConsumer_A<B>' requested here}} expected-note {{bad instantiation of 'PickyConsumer_A<E>' requested here}} expected-note {{bad instantiation of 'PickyConsumer_A<F>' requested here}} expected-note {{bad instantiation of 'PickyConsumer_A<G>' requested here}}
};

template <class T>
struct MOZ_NEEDS_NO_VTABLE_TYPE PickyConsumer_B { // expected-error {{'PickyConsumer_B<B>' cannot be instantiated because 'B' has a VTable}} expected-error {{'PickyConsumer_B<E>' cannot be instantiated because 'E' has a VTable}} expected-error {{'PickyConsumer_B<F>' cannot be instantiated because 'F' has a VTable}} expected-error {{'PickyConsumer_B<G>' cannot be instantiated because 'G' has a VTable}}
  T *m;
};
template <class T>
struct PickyConsumerSubclass : PickyConsumer_B<T> {}; // expected-note {{bad instantiation of 'PickyConsumer_B<B>' requested here}} expected-note {{bad instantiation of 'PickyConsumer_B<E>' requested here}} expected-note {{bad instantiation of 'PickyConsumer_B<F>' requested here}} expected-note {{bad instantiation of 'PickyConsumer_B<G>' requested here}}

template <class T>
struct NonPickyConsumer {
  T *m;
};

struct A {};
struct B : virtual A {};
struct C : A {};
struct D {
  void d();
};
struct E {
  virtual void e();
};
struct F : E {
  virtual void e() final override;
};
struct G {
  virtual void e() = 0;
};

void f() {
  {
    PickyConsumer<A> a1;
    PickyConsumerWrapper<A> a2;
    PickyConsumerSubclass<A> a3;
    NonPickyConsumer<A> a4;
  }

  {
    PickyConsumer<B> a1; // expected-note {{bad instantiation of 'PickyConsumer<B>' requested here}}
    PickyConsumerWrapper<B> a2;
    PickyConsumerSubclass<B> a3;
    NonPickyConsumer<B> a4;
  }

  {
    PickyConsumer<C> a1;
    PickyConsumerWrapper<C> a2;
    PickyConsumerSubclass<C> a3;
    NonPickyConsumer<C> a4;
  }

  {
    PickyConsumer<D> a1;
    PickyConsumerWrapper<D> a2;
    PickyConsumerSubclass<D> a3;
    NonPickyConsumer<D> a4;
  }

  {
    PickyConsumer<E> a1; // expected-note {{bad instantiation of 'PickyConsumer<E>' requested here}}
    PickyConsumerWrapper<E> a2;
    PickyConsumerSubclass<E> a3;
    NonPickyConsumer<E> a4;
  }

  {
    PickyConsumer<F> a1; // expected-note {{bad instantiation of 'PickyConsumer<F>' requested here}}
    PickyConsumerWrapper<F> a2;
    PickyConsumerSubclass<F> a3;
    NonPickyConsumer<F> a4;
  }

  {
    PickyConsumer<G> a1; // expected-note {{bad instantiation of 'PickyConsumer<G>' requested here}}
    PickyConsumerWrapper<G> a2;
    PickyConsumerSubclass<G> a3;
    NonPickyConsumer<G> a4;
  }
}
