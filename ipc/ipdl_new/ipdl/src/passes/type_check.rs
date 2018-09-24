/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::collections::{HashMap, HashSet};
use ast::*;
use errors::Errors;


const BUILTIN_TYPES: &[&str] = &[
    // C types
    "bool",
    "char",
    "short",
    "int",
    "long",
    "float",
    "double",

    // stdint types
    "int8_t",
    "uint8_t",
    "int16_t",
    "uint16_t",
    "int32_t",
    "uint32_t",
    "int64_t",
    "uint64_t",
    "intptr_t",
    "uintptr_t",

    // stddef types
    "size_t",
    "ssize_t",

    // Mozilla types: "less" standard things we know how serialize/deserialize
    "nsresult",
    "nsString",
    "nsCString",
    "nsDependentSubstring",
    "nsDependentCSubstring",
    "mozilla::ipc::Shmem",
    "mozilla::ipc::ByteBuf",
    "mozilla::ipc::FileDescriptor",
];

fn builtin_from_string(tname: &str) -> TypeSpec {
    TypeSpec::new(QualifiedId::new_from_iter(tname.split("::")))
}

const DELETE_MESSAGE_NAME: &str = "__delete__";
const CONSTRUCTOR_SUFFIX: &str = "Constructor";


#[derive(Debug, Clone, PartialEq, Eq, Hash)]
struct TypeRef {
    tu: TUId,
    index: usize,
}

impl TypeRef {
    fn new(tu: TUId, index: usize) -> TypeRef {
        TypeRef { tu, index }
    }

    fn lookup_struct<'a>(&self,
                         tuts: &'a HashMap<TUId, TranslationUnitType>) -> &'a StructDef {
        &tuts.get(&self.tu).unwrap().structs[self.index]
    }

    fn lookup_union<'a>(&self,
                         tuts: &'a HashMap<TUId, TranslationUnitType>) -> &'a UnionDef {
        &tuts.get(&self.tu).unwrap().unions[self.index]
    }
}

// XXX The Python compiler has "Type" and a subclass "IPDLType". I
// don't know how useful it is to split them. Plus my notion of type
// may be different.
#[derive(Debug, Clone)]
enum IPDLType {
    ImportedCxx(QualifiedId, bool /* refcounted */),
    Message(TypeRef),
    Protocol(TUId),
    Actor(TUId, bool /* nullable */),
    Struct(TypeRef),
    Union(TypeRef),
    Array(Box<IPDLType>),
    Shmem(QualifiedId),
    ByteBuf(QualifiedId),
    FD(QualifiedId),
    Endpoint(QualifiedId),
}


impl IPDLType {
    fn type_name(&self) -> &'static str {
        match self {
            IPDLType::ImportedCxx(_, _) => "imported C++ type",
            IPDLType::Message(_) => "message type",
            IPDLType::Protocol(_) => "protocol type",
            IPDLType::Actor(_, _) => "actor type",
            IPDLType::Struct(_) => "struct type",
            IPDLType::Union(_) => "union type",
            IPDLType::Array(_) => "array type",
            IPDLType::Shmem(_) => "shmem type",
            IPDLType::ByteBuf(_) => "bytebuf type",
            IPDLType::FD(_) => "fd type",
            IPDLType::Endpoint(_) => "endpoint type",
        }
    }

    fn canonicalize(&self, type_spec: &TypeSpec) -> (Errors, IPDLType) {
        let mut errors = Errors::none();
        let mut itype = self.clone();

        if let IPDLType::Protocol(p) = *self {
            itype = IPDLType::Actor(p, type_spec.nullable)
        }

        match itype {
            IPDLType::Actor(_, _) => (),
            _ => {
                if type_spec.nullable {
                    errors.append_one(type_spec.loc(),
                                      &format!("`nullable' qualifier for {} makes no sense", itype.type_name()));
                }
            }
        }

        if type_spec.array {
            itype = IPDLType::Array(Box::new(itype))
        }

        (errors, itype)
    }

    fn is_refcounted(&self) -> bool {
        match *self {
            IPDLType::ImportedCxx(_, refcounted) => refcounted,
            _ => false,
        }
    }
}

#[derive(Debug, Clone)]
struct StructDef {
    qname: QualifiedId,
    fields: Vec<IPDLType>,
}

impl StructDef {
    fn new(ns: &Namespace) -> StructDef {
        StructDef { qname: ns.qname(), fields: Vec::new() }
    }

    fn append_field(&mut self, field_type: IPDLType) {
        self.fields.push(field_type)
    }
}

#[derive(Debug, Clone)]
struct UnionDef {
    qname: QualifiedId,
    components: Vec<IPDLType>,
}

impl UnionDef {
    fn new(ns: &Namespace) -> UnionDef {
        UnionDef { qname: ns.qname(), components: Vec::new() }
    }

    fn append_component(&mut self, component_type: IPDLType) {
        self.components.push(component_type)
    }
}

#[derive(Debug, Clone)]
enum Message {
    Ctor(TUId),
    Dtor(TUId),
    Other,
}

impl Message {
    fn is_ctor(&self) -> bool {
        match self {
            Message::Ctor(_) => true,
            _ => false,
        }
    }

    fn constructed_type(&self) -> TUId {
        match *self {
            Message::Ctor(tuid) => tuid,
            _ => panic!("Tried to get constructed type on non-Ctor"),
        }
    }

    fn is_dtor(&self) -> bool {
        match self {
            Message::Dtor(_) => true,
            _ => false,
        }
    }
}


struct MessageStrength {
    send_semantics: SendSemantics,
    nested_min: Nesting,
    nested_max: Nesting,
}

impl MessageStrength {
    fn converts_to(&self, other: &MessageStrength) -> bool {
        if self.nested_min < other.nested_min {
            return false;
        }

        if self.nested_max > other.nested_max {
            return false;
        }

        // Protocols that use intr semantics are not allowed to use
        // message nesting.
        if other.send_semantics.is_intr() {
            return self.nested_min.is_none() && self.nested_max.is_none();
        }

        match self.send_semantics {
            SendSemantics::Async => true,
            SendSemantics::Sync => !other.send_semantics.is_async(),
            SendSemantics::Intr => false,
        }
    }
}

#[derive(Debug, Clone)]
struct ParamTypeDef {
    name: Identifier,
    param_type: IPDLType,
}

#[derive(Debug, Clone)]
struct MessageDef {
    name: Identifier,
    send_semantics: SendSemantics,
    nested: Nesting,
    prio: Priority,
    direction: Direction,
    params: Vec<ParamTypeDef>,
    returns: Vec<ParamTypeDef>,
    mtype: Message,
    compress: Compress,
    verify: bool,
}

impl MessageDef {
    fn new(md: &MessageDecl, name: &str, mtype: Message) -> MessageDef {
        assert!(!mtype.is_ctor() || name.ends_with(CONSTRUCTOR_SUFFIX));
        MessageDef {
            name: Identifier::new(String::from(name), md.name.loc.clone()),
            send_semantics: md.send_semantics, nested: md.nested, prio: md.prio,
            direction: md.direction, params: Vec::new(), returns: Vec::new(),
            mtype, compress: md.compress, verify: md.verify
        }
    }

    fn is_ctor(&self) -> bool {
        self.mtype.is_ctor()
    }

    fn constructed_type(&self) -> TUId {
        self.mtype.constructed_type()
    }

    fn is_dtor(&self) -> bool {
        self.mtype.is_dtor()
    }

    fn message_strength(&self) -> MessageStrength {
        MessageStrength {
            send_semantics: self.send_semantics,
            nested_min: self.nested,
            nested_max: self.nested,
        }
    }

    fn converts_to(&self, protocol: &ProtocolDef) -> bool {
        self.message_strength().converts_to(&protocol.message_strength())
    }

    pub fn is_async(&self) -> bool {
        self.send_semantics.is_async()
    }

    pub fn is_sync(&self) -> bool {
        self.send_semantics.is_sync()
    }

    pub fn is_intr(&self) -> bool {
        self.send_semantics.is_intr()
    }
}

#[derive(Debug, Clone)]
struct ProtocolDef {
    qname: QualifiedId,
    send_semantics: SendSemantics,
    nested: Nesting,
    managers: Vec<TUId>,
    manages: Vec<TUId>,
    messages: Vec<MessageDef>,
    has_delete: bool,
    has_reentrant_delete: bool,
}

impl ProtocolDef {
    fn new(&(ref ns, ref p): &(Namespace, Protocol)) -> ProtocolDef {
        ProtocolDef {
            qname: ns.qname(), send_semantics: p.send_semantics, nested: p.nested,
            managers: Vec::new(), manages: Vec::new(), messages: Vec::new(),
            has_delete: false, has_reentrant_delete: false,
        }
    }

    fn is_top_level(&self) -> bool {
        self.managers.is_empty()
    }

    fn message_strength(&self) -> MessageStrength {
        MessageStrength {
            send_semantics: self.send_semantics,
            nested_min: Nesting::None,
            nested_max: self.nested,
        }
    }

    fn converts_to(&self, other: &ProtocolDef) -> bool {
        self.message_strength().converts_to(&other.message_strength())
    }
}

#[derive(Debug, Clone)]
struct Decl {
    loc: Location,
    decl_type: IPDLType,
    short_name: String,
    full_name: Option<String>,
}

// The Python version also has a "progname" field, but I don't see any
// reason to keep that separate from the short_name field.

impl Decl {
    fn new(loc: &Location, decl_type: IPDLType, short_name: String) -> Decl {
        Decl {
            loc: loc.clone(), decl_type,
            short_name, full_name: None
        }
    }

    fn new_from_qid(qid: &QualifiedId, decl_type: IPDLType) -> Decl {
        Decl {
            loc: qid.loc().clone(), decl_type,
            short_name: qid.short_name(), full_name: qid.full_name()
        }
    }
}

struct SymbolTable {
    scopes: Vec<HashMap<String, Decl>>
}

impl SymbolTable {
    fn new() -> SymbolTable {
        SymbolTable { scopes: vec![HashMap::new()] }
    }

    fn enter_scope(&mut self) {
        self.scopes.push(HashMap::new())
    }

    fn exit_scope(&mut self) {
        self.scopes.pop().unwrap();
        ()
    }

    // XXX Should/can this return a reference?
    fn lookup(&self, sym: &str) -> Option<Decl> {
        for s in &self.scopes {
            if let Some(e) = s.get(sym) {
                return Some(e.clone())
            }
        }
        None
    }

    fn declare_inner(&mut self, name: &str, decl: Decl) -> Errors {
        if let Some(old_decl) = self.lookup(name) {
            return Errors::one(&decl.loc,
                               &format!("redeclaration of symbol `{}', first declared at {}",
                                        name, old_decl.loc))
        }

        let old_binding = self.scopes.last_mut().unwrap().insert(String::from(name), decl);
        assert!(old_binding.is_none());
        Errors::none()
    }

    fn declare(&mut self, decl: &Decl) -> Errors {
        let mut errors = self.declare_inner(&decl.short_name, decl.clone());
        if let Some(ref full_name) = decl.full_name {
            errors.append(self.declare_inner(full_name, decl.clone()));
        }
        errors
    }
}

fn declare_cxx_type(sym_tab: &mut SymbolTable, cxx_type: &TypeSpec, refcounted: bool) -> Errors {
    let ipdl_type = match cxx_type.spec.full_name() {
        Some(ref n) if n == "mozilla::ipc::Shmem" =>
            IPDLType::Shmem(cxx_type.spec.clone()),
        Some(ref n) if n == "mozilla::ipc::ByteBuf" =>
            IPDLType::ByteBuf(cxx_type.spec.clone()),
        Some(ref n) if n == "mozilla::ipc::FileDescriptor" =>
            IPDLType::FD(cxx_type.spec.clone()),
        _ => {
            let ipdl_type = IPDLType::ImportedCxx(cxx_type.spec.clone(), refcounted);
            let full_name = format!("{}", cxx_type.spec);
            if let Some(decl) = sym_tab.lookup(&full_name) {
                if let Some(existing_type) = decl.full_name {
                    if existing_type == full_name {
                        if refcounted != decl.decl_type.is_refcounted() {
                            return Errors::one(&cxx_type.loc(),
                                               &format!("inconsistent refcounted status of type `{}', first declared at {}",
                                                        full_name, decl.loc))
                        }
                        // This type has already been added, so don't do anything.
                        return Errors::none()
                    }
                };
            };
            ipdl_type
        },
    };
    sym_tab.declare(&Decl::new_from_qid(&cxx_type.spec, ipdl_type))
}

struct TranslationUnitType {
    pub structs: Vec<StructDef>,
    pub unions: Vec<UnionDef>,
    pub protocol: Option<ProtocolDef>,
}

impl TranslationUnitType {
    fn new(maybe_protocol: &Option<(Namespace, Protocol)>) -> TranslationUnitType {
        let protocol = maybe_protocol.as_ref().map(|ref p| ProtocolDef::new(&p));
        TranslationUnitType { structs: Vec::new(), unions: Vec::new(), protocol }
    }
}


fn declare_protocol(sym_tab: &mut SymbolTable,
                    tuid: TUId,
                    ns: &Namespace) -> Errors {
    let mut errors = Errors::none();

    let p_type = IPDLType::Protocol(tuid);
    errors.append(sym_tab.declare(&Decl::new_from_qid(&ns.qname(), p_type)));

    let loc = &ns.name.loc;
    let mut declare_endpoint = |side: String| {
        let full_id = Identifier::new(format!("Endpoint<{}{}>", ns.qname(), side), loc.clone());
        let namespaces = vec!["mozilla".to_string(), "ipc".to_string()];
        let full_qid = QualifiedId { base_id: full_id, quals: namespaces };
        let short_name = format!("Endpoint<{}{}>", ns.name.id, side);
        sym_tab.declare(&Decl::new(loc, IPDLType::Endpoint(full_qid), short_name))
    };
    errors.append(declare_endpoint("Parent".to_string()));
    errors.append(declare_endpoint("Child".to_string()));

    errors
}


fn declare_usings(mut sym_tab: &mut SymbolTable,
                  tu: &TranslationUnit) -> Errors {
    let mut errors = Errors::none();
    for u in &tu.using {
        errors.append(declare_cxx_type(&mut sym_tab, &u.cxx_type, u.refcounted));
    }
    errors
}


fn declare_structs_and_unions(sym_tab: &mut SymbolTable,
                              tuid: TUId,
                              tu: &TranslationUnit) -> Errors {
    let mut errors = Errors::none();
    let mut index = 0;

    for s in &tu.structs {
        let s_type = IPDLType::Struct(TypeRef::new(tuid, index));
        errors.append(sym_tab.declare(&Decl::new_from_qid(&s.0.qname(), s_type)));
        index += 1;
    }

    index = 0;
    for u in &tu.unions {
        let u_type = IPDLType::Union(TypeRef::new(tuid, index));
        errors.append(sym_tab.declare(&Decl::new_from_qid(&u.0.qname(), u_type)));
        index += 1;
    }

    errors
}


fn gather_decls_struct(sym_tab: &mut SymbolTable,
                       &(ref ns, ref sd): &(Namespace, Vec<StructField>),
                       sdef: &mut StructDef) -> Errors {
    let mut errors = Errors::none();

    sym_tab.enter_scope();

    for f in sd {
        let fty_string = f.type_spec.spec.to_string();
        let fty_decl = sym_tab.lookup(&fty_string);
        if fty_decl.is_none() {
            errors.append_one(&f.name.loc,
                              &format!("field `{}' of struct `{}' has unknown type `{}'",
                                       f.name, ns.qname().short_name(), fty_string));
            continue;
        }
        let (errors2, f_type) = fty_decl.unwrap().decl_type.canonicalize(&f.type_spec);
        errors.append(errors2);

        errors.append(sym_tab.declare(&Decl::new(&f.name.loc, f_type.clone(), f.name.id.clone())));
        sdef.append_field(f_type);
    }

    sym_tab.exit_scope();

    errors
}


fn gather_decls_union(sym_tab: &mut SymbolTable,
                      &(ref ns, ref ud): &(Namespace, Vec<TypeSpec>),
                      udef: &mut UnionDef) -> Errors {
    let mut errors = Errors::none();

    for c in ud {
        let c_string = c.spec.to_string();
        let c_decl = sym_tab.lookup(&c_string);
        if c_decl.is_none() {
            errors.append_one(c.loc(),
                              &format!("unknown component type `{}' of union `{}'",
                                       c_string, ns.qname().short_name()));
            continue;
        }
        let (errors2, c_ty) = c_decl.unwrap().decl_type.canonicalize(&c);
        errors.append(errors2);
        udef.append_component(c_ty);
    }

    errors
}


fn gather_decls_manager(sym_tab: &mut SymbolTable,
                      managee: &(Namespace, Protocol),
                      managee_type: &mut ProtocolDef,
                      manager: &Identifier) -> Errors {
    let manager_decl = match sym_tab.lookup(&manager.id) {
        Some(decl) => decl,
        None => return Errors::one(&manager.loc,
                                   &format!("protocol `{}' referenced as |manager| of `{}' has not been declared",
                                            manager.id, managee.0.qname().short_name())),
    };

    if let IPDLType::Protocol(ref pt) = &manager_decl.decl_type {
        managee_type.managers.push(pt.clone());
        return Errors::none();
    }

    Errors::one(&manager.loc,
                &format!("entity `{}' referenced as |manager| of `{}' is not of `protocol' type; instead it is a {}",
                        manager.id, managee.0.qname().short_name(),
                        manager_decl.decl_type.type_name()))
}


fn gather_decls_manages(sym_tab: &mut SymbolTable,
                        manager: &(Namespace, Protocol),
                        manager_type: &mut ProtocolDef,
                        managee: &Identifier) -> Errors {

    let managee_decl = match sym_tab.lookup(&managee.id) {
        Some(decl) => decl,
        None => return Errors::one(&managee.loc,
                                   &format!("protocol `{}', managed by `{}', has not been declared",
                                            managee.id, manager.0.qname().short_name())),
    };

    if let IPDLType::Protocol(ref pt) = &managee_decl.decl_type {
        manager_type.manages.push(pt.clone());
        return Errors::none();
    }

    Errors::one(&managee.loc,
                &format!("{} declares itself managing a non-`protocol' entity `{}' that is a {}",
                        manager.0.qname().short_name(), managee.id, managee_decl.decl_type.type_name()))
}


fn gather_decls_message(sym_tab: &mut SymbolTable,
                        tuid: TUId,
                        protocol_type: &mut ProtocolDef,
                        md: &MessageDecl) -> Errors {
    let mut errors = Errors::none();
    let mut message_name = md.name.id.clone();
    let mut mtype = Message::Other;

    if let Some(ref decl) = sym_tab.lookup(&message_name) {
        if let IPDLType::Protocol(pt) = decl.decl_type {
            // Probably a ctor. We'll check validity later.
            message_name += CONSTRUCTOR_SUFFIX;
            mtype = Message::Ctor(pt);
        } else {
            errors.append_one(&md.name.loc,
                              &format!("message name `{}' already declared as `{}'",
                                       md.name, decl.decl_type.type_name()));
            // If we error here, no big deal; move on to find more.
        }
    }

    if DELETE_MESSAGE_NAME == message_name {
        mtype = Message::Dtor(tuid);
    }

    sym_tab.enter_scope();

    let mut msg_type = MessageDef::new(&md, &message_name, mtype);

    {
        // The Python version adds the parameter, just with a dummy
        // type. Here I choose to be consistent with how we handle struct
        // fields with invalid types and simply omit the parameter.
        let mut param_to_decl = |param: &Param| {
            let pt_name = param.type_spec.spec.to_string();
            match sym_tab.lookup(&pt_name) {
                Some(p_type) => {
                    let (errors2, t) = p_type.decl_type.canonicalize(&param.type_spec);
                    errors.append(errors2);
                    let decl = Decl::new(param.type_spec.loc(), t.clone(), param.name.id.clone());
                    errors.append(sym_tab.declare(&decl));
                    Some(ParamTypeDef { name: param.name.clone(), param_type: t })
                }
                None => {
                    errors.append_one(param.type_spec.loc(),
                                      &format!("argument typename `{}' of message `{}' has not been declared",
                                               &pt_name, message_name));
                    None
                }
            }
        };

        for in_param in &md.in_params {
            if let Some(t) = param_to_decl(&in_param) {
                msg_type.params.push(t);
            }
        }

        for out_param in &md.out_params {
            if let Some(t) = param_to_decl(&out_param) {
                msg_type.returns.push(t);
            }
        }
    }

    sym_tab.exit_scope();

    let index = protocol_type.messages.len();
    protocol_type.messages.push(msg_type);

    let mt = IPDLType::Message(TypeRef::new(tuid, index));
    errors.append(sym_tab.declare(&Decl::new(&md.name.loc, mt, message_name)));

    errors
}

fn gather_decls_protocol(mut sym_tab: &mut SymbolTable,
                         tuid: TUId,
                         p: &(Namespace, Protocol),
                         mut p_type: &mut ProtocolDef) -> Errors {
    let mut errors = Errors::none();

    sym_tab.enter_scope();

    {
        let mut seen_managers = HashSet::new();
        for manager in &p.1.managers {
            if seen_managers.contains(&manager.id) {
                errors.append_one(&manager.loc,
                                  &format!("manager `{}' appears multiple times",
                                           manager.id));
                continue;
            }

            seen_managers.insert(manager.id.clone());

            errors.append(gather_decls_manager(&mut sym_tab, &p, &mut p_type, &manager));
        }
    }

    for managee in &p.1.manages {
        errors.append(gather_decls_manages(&mut sym_tab, &p, &mut p_type, &managee));
    }

    if p.1.managers.is_empty() && p.1.messages.is_empty() {
        errors.append_one(&p.0.name.loc,
                          &format!("top-level protocol `{}' cannot be empty",
                                   p.0.qname().short_name()));
    }

    for md in &p.1.messages {
        errors.append(gather_decls_message(&mut sym_tab, tuid, &mut p_type, &md));
    }

    let delete_type = sym_tab.lookup(DELETE_MESSAGE_NAME);
    p_type.has_delete = delete_type.is_some();
    if !(p_type.has_delete || p_type.is_top_level()) {
        errors.append_one(&p.0.name.loc,
                          &format!("destructor declaration `{}(...)' required for managed protocol `{}'",
                                   DELETE_MESSAGE_NAME , p.0.qname().short_name()));
    }

    p_type.has_reentrant_delete = match delete_type {
        Some(decl) => match decl.decl_type {
            IPDLType::Message(tr) =>
                p_type.messages[tr.index].is_intr(),
            _ => panic!("Invalid message type for delete message"),
        },
        None => false
    };

    for managed in &p.1.manages {
        let ctor_name = managed.id.clone() + CONSTRUCTOR_SUFFIX;
        if let Some(Decl { decl_type: IPDLType::Message(tr), .. }) = sym_tab.lookup(&ctor_name) {
            if p_type.messages[tr.index].is_ctor() {
                continue;
            }
        }
        errors.append_one(&managed.loc,
                          &format!("constructor declaration required for managed protocol `{}' (managed by protocol `{}')",
                                   managed.id, p.0.qname().short_name()));
    }

    // FIXME/cjones Declare all the little C++ thingies that will
    // be generated. They're not relevant to IPDL itself, but
    // those ("invisible") symbols can clash with others in the
    // IPDL spec, and we'd like to catch those before C++ compilers
    // are allowed to obfuscate the error.

    sym_tab.exit_scope();

    errors
}


fn gather_decls_tu(tus: &HashMap<TUId, TranslationUnit>,
                   tuts: &mut HashMap<TUId, TranslationUnitType>,
                   tuid: TUId,
                   tu: &TranslationUnit) -> Result<(), Errors> {
    let mut errors = Errors::none();
    let mut sym_tab = SymbolTable::new();
    let tut = &mut tuts.get_mut(&tuid).unwrap();

    if let Some(ref p) = &tu.protocol {
        errors.append(declare_protocol(&mut sym_tab, tuid, &p.0));
    }

    // Add the declarations from all the IPDL files we include.
    for &include_tuid in &tu.includes {
        let include_tu = tus.get(&include_tuid).unwrap();
        match include_tu.protocol {
            Some(ref p) =>
                errors.append(declare_protocol(&mut sym_tab, include_tuid, &p.0)),
            None => {
                // This is a header.  Import its "exported" globals into our scope.
                errors.append(declare_usings(&mut sym_tab, &include_tu));
                errors.append(declare_structs_and_unions(&mut sym_tab, include_tuid, &include_tu));
            },
        }
    }

    // Declare builtin C++ types.
    for t in BUILTIN_TYPES {
        let cxx_type = builtin_from_string(t);
        errors.append(declare_cxx_type(&mut sym_tab, &cxx_type, false /* refcounted */));
    }

    // Declare imported C++ types.
    errors.append(declare_usings(&mut sym_tab, &tu));

    // Create stubs for top level struct and union decls.
    for s in &tu.structs {
        tut.structs.push(StructDef::new(&s.0));
    }
    for s in &tu.unions {
        tut.unions.push(UnionDef::new(&s.0));
    }

    // Forward declare all structs and unions in order to support
    // recursive definitions.
    errors.append(declare_structs_and_unions(&mut sym_tab, tuid, &tu));

    // Check definitions of structs and unions.
    // XXX It might be cleaner to do a zip iteration over {tu,tut}.structs
    let mut index = 0;
    for su in &tu.structs {
        errors.append(gather_decls_struct(&mut sym_tab, &su, &mut tut.structs[index]));
        index += 1;
    }
    index = 0;
    for u in &tu.unions {
        errors.append(gather_decls_union(&mut sym_tab, &u, &mut tut.unions[index]));
        index += 1;
    }

    // The Python version type checks every struct and union included
    // from an ipdlh file here, but I don't think that makes any
    // sense.

    if let Some(ref p) = &tu.protocol {
        errors.append(gather_decls_protocol(&mut sym_tab, tuid, &p, &mut tut.protocol.as_mut().unwrap()));
    }

    if errors.is_empty() { Ok(()) } else { Err(errors) }
}


enum FullyDefinedState {
    Visiting,
    Defined(bool),
}

#[derive(Debug, Clone, PartialEq, Eq, Hash)]
enum CompoundType {
    Struct,
    Union,
}


/* The rules for "full definition" of a type are
     defined(atom)             := true
     defined(array basetype)   := defined(basetype)
     defined(struct f1 f2...)  := defined(f1) and defined(f2) and ...
     defined(union c1 c2 ...)  := defined(c1) or defined(c2) or ...
 */
fn fully_defined(tuts: &HashMap<TUId, TranslationUnitType>,
                 mut defined: &mut HashMap<(CompoundType, TypeRef), FullyDefinedState>,
                 t: &IPDLType) -> bool {

    let key = match t {
        IPDLType::Struct(ref tr) => (CompoundType::Struct, tr.clone()),
        IPDLType::Union(ref tr) => (CompoundType::Union, tr.clone()),
        IPDLType::Array(ref t_inner) => return fully_defined(&tuts, &mut defined, &t_inner),
        _ => return true,
    };

    // The Python version would repeatedly visit a type that was found
    // to be not defined. I think that's unnecessary. Not doing it
    // might save some time in the case of an error.

    if let Some(state) = defined.get(&key) {
        return match *state {
            FullyDefinedState::Visiting => false,
            FullyDefinedState::Defined(is_defined) => is_defined,
        }
    }

    defined.insert(key.clone(), FullyDefinedState::Visiting);

    let mut is_defined;
    match key.0 {
        CompoundType::Struct => {
            is_defined = true;
            for f in &key.1.lookup_struct(&tuts).fields {
                if !fully_defined(&tuts, &mut defined, f) {
                    is_defined = false;
                    break
                }
            }
        },
        CompoundType::Union => {
            is_defined = false;
            for f in &key.1.lookup_union(&tuts).components {
                if fully_defined(&tuts, &mut defined, f) {
                    is_defined = true;
                    break
                }
            }
        },
    }

    // XXX Don't need to insert here. get_mut should work.
    defined.insert(key, FullyDefinedState::Defined(is_defined));

    is_defined
}


enum ManagerCycleState {
    Visiting,
    Acyclic,
}


fn get_protocol_type<'a>(tuts: &'a HashMap<TUId, TranslationUnitType>,
                         tuid: &TUId) -> &'a ProtocolDef
{
    tuts.get(tuid).unwrap().protocol.as_ref().unwrap()
}

fn manager_cycle_error(tuts: &HashMap<TUId, TranslationUnitType>,
                       v: &[TUId],
                       tuid: &TUId) -> Errors {
    let mut errors = Errors::none();
    let mut found = false;
    for p in v {
        if !found {
            if p != tuid {
                continue;
            }
            errors.append_one(get_protocol_type(&tuts, &tuid).qname.loc(),
                              "cycle detected in manager/manages hierarchy:");
            found = true;
        }
        let pt = get_protocol_type(&tuts, &p);
        errors.append_one(pt.qname.loc(),
                          &format!("\t{}", pt.qname));
    }
    assert!(found);
    errors
}

fn protocol_managers_acyclic(tuts: &HashMap<TUId, TranslationUnitType>,
                             mut visited: &mut HashMap<TUId, ManagerCycleState>,
                             mut stack: &mut Vec<TUId>,
                             tuid: &TUId) -> Errors {
    if let Some(state) = visited.get(tuid) {
        return match state {
            ManagerCycleState::Visiting => manager_cycle_error(&tuts, &stack, tuid),
            ManagerCycleState::Acyclic => Errors::none(),
        }
    }

    let mut errors = Errors::none();
    visited.insert(tuid.clone(), ManagerCycleState::Visiting);

    stack.push(tuid.clone());

    let pt = get_protocol_type(&tuts, &tuid);
    for managee in &pt.manages {
        // Self-managed protocols are allowed, except at the top level.
        // The top level case is checked in protocol_managers_acyclic.
        if managee == tuid {
            continue;
        }

        errors.append(protocol_managers_acyclic(&tuts, &mut visited, &mut stack, &managee));
    }

    stack.pop();

    *visited.get_mut(tuid).unwrap() = ManagerCycleState::Acyclic;

    errors
}

fn protocols_managers_acyclic(tuts: &HashMap<TUId, TranslationUnitType>) -> Errors {
    let mut errors = Errors::none();
    let mut visited = HashMap::new();
    let mut stack = Vec::new();

    for (tuid, tut) in tuts {
        if tut.protocol.is_none() {
            continue;
        }

        errors.append(protocol_managers_acyclic(&tuts, &mut visited, &mut stack, &tuid));

        let pt = get_protocol_type(&tuts, &tuid);
        if pt.managers.len() == 1 && &pt.managers[0] == tuid {
            errors.append_one(pt.qname.loc(),
                              &format!("top-level protocol `{}' cannot manage itself",
                                      pt.qname.short_name()));
        }
    }
    errors
}

fn check_types_message(ptype: &ProtocolDef,
                       mtype: &MessageDef) -> Errors {
    let mut errors = Errors::none();
    let mname = &mtype.name.id;

    if mtype.nested.inside_sync() && !mtype.is_sync() {
        errors.append_one(&mtype.name.loc,
                          &format!("inside_sync nested messages must be sync (here, message `{}' in protocol `{}')",
                                   mname, ptype.qname.short_name()));
    }

    let is_to_child = mtype.direction.is_to_child() || mtype.direction.is_both();

    if mtype.nested.inside_cpow() && is_to_child {
        errors.append_one(&mtype.name.loc,
                          &format!("inside_cpow nested parent-to-child messages are verboten (here, message `{}' in protocol `{}')",
                                   mname, ptype.qname.short_name()));
    }

    // We allow inside_sync messages that are themselves sync to be sent from the
    // parent. Normal and inside_cpow nested messages that are sync can only come from
    // the child.
    if mtype.is_sync() && mtype.nested.is_none() && is_to_child {
        errors.append_one(&mtype.name.loc,
                          &format!("sync parent-to-child messages are verboten (here, message `{}' in protocol `{}')",
                                   mname, ptype.qname.short_name()));
    }

    if !mtype.converts_to(&ptype) {
        errors.append_one(&mtype.name.loc,
                          &format!("message `{}' requires more powerful send semantics than its protocol `{}' provides",
                                   mname, ptype.qname.short_name()));
    }

    if (mtype.is_ctor() || mtype.is_dtor()) && mtype.is_async() && !mtype.returns.is_empty() {
        errors.append_one(&mtype.name.loc,
                          &format!("asynchronous ctor/dtor message `{}' in protocol `{}' declares return values",
                                   mname, ptype.qname.short_name()));
    }

    if mtype.compress != Compress::None && (!mtype.is_async() || mtype.is_ctor() || mtype.is_dtor()) {
        let pname = ptype.qname.short_name();
        let message = if mtype.is_ctor() || mtype.is_dtor() {
            let message_type = if mtype.is_ctor() { "constructor" } else { "destructor" };
            format!("{} messages can't use compression (here, in protocol `{}')",
                              message_type, pname)
        } else {
            format!("message `{}' in protocol `{}' requests compression but is not async",
                              mname, pname)
        };

        errors.append_one(&mtype.name.loc, &message);
    }

    if mtype.is_ctor() && !ptype.manages.contains(&mtype.constructed_type()) {
        let ctor_protocol_len = mname.len() - CONSTRUCTOR_SUFFIX.len();
        errors.append_one(&mtype.name.loc,
                          &format!("ctor for protocol `{}', which is not managed by protocol `{}'",
                                   &mname[0..ctor_protocol_len],
                                   ptype.qname.short_name()));
    }

    errors
}

fn check_types_protocol(tuts: &HashMap<TUId, TranslationUnitType>,
                        tuid: &TUId,
                        ptype: &ProtocolDef) -> Errors {
    let mut errors = protocols_managers_acyclic(&tuts);

    for manager in &ptype.managers {
        let manager_type = get_protocol_type(&tuts, &manager);
        if !ptype.converts_to(&manager_type) {
            errors.append_one(&ptype.qname.loc(),
                              &format!("protocol `{}' requires more powerful send semantics than its manager `{}' provides",
                                       ptype.qname.short_name(), manager_type.qname.short_name()));
        }

        if !manager_type.manages.contains(&tuid) {
            errors.append_one(&manager_type.qname.loc(),
                              &format!("|manager| declaration in protocol `{}' does not match any |manages| declaration in protocol `{}'",
                                       ptype.qname.short_name(), manager_type.qname.short_name()));
        }
    }

    for managee in &ptype.manages {
        let managee_type = get_protocol_type(&tuts, &managee);

        if !managee_type.managers.contains(&tuid) {
            errors.append_one(&managee_type.qname.loc(),
                              &format!("|manages| declaration in protocol `{}' does not match any |manager| declaration in protocol `{}'",
                                       ptype.qname.short_name(), managee_type.qname.short_name()));
        }
    }

    for mtype in &ptype.messages {
        errors.append(check_types_message(&ptype, &mtype));
    }

    errors
}


fn check_types_tu(tus: &HashMap<TUId, TranslationUnit>,
                  tuts: &HashMap<TUId, TranslationUnitType>,
                  mut defined: &mut HashMap<(CompoundType, TypeRef), FullyDefinedState>,
                  tuid: TUId,
                  tut: &TranslationUnitType) -> Result<(), Errors> {
    let mut errors = Errors::none();

    let tu = tus.get(&tuid).unwrap();

    for i in 0..tut.structs.len() {
        if !fully_defined(&tuts, &mut defined,
                          &IPDLType::Struct(TypeRef::new(tuid, i))) {
            errors.append_one(&tu.structs[i].0.name.loc,
                              &format!("struct `{}' is only partially defined",
                                       &tu.structs[i].0.name.id));
        }
    }

    for i in 0..tut.unions.len() {
        if !fully_defined(&tuts, &mut defined,
                          &IPDLType::Union(TypeRef::new(tuid, i))) {
            errors.append_one(&tu.unions[i].0.name.loc,
                              &format!("union `{}' is only partially defined",
                                       &tu.unions[i].0.name.id));
        }
    }

    if let Some(ref pt) = &tut.protocol {
        errors.append(check_types_protocol(&tuts, &tuid, &pt));
    }

    // XXX We don't need to track visited because we will visited all
    // translation units at the top level.

    // XXX What is "ptype"? In Python, it is set to None at the top of this method.

    // XXX The Python checker calls visitIncludes on tu.includes,
    // which checks any included protocols. I don't know why that
    // would be useful.


    if errors.is_empty() { Ok(()) } else { Err(errors) }
}


// Basic checking that doesn't relate to types specifically.
pub fn check_translation_unit(tu: &TranslationUnit) -> Result<(), Errors> {
    if let Some((ref ns, _)) = &tu.protocol {

        // For a protocol file, the filename should match the
        // protocol. (In the Python IPDL compiler, translation units have
        // a separate "name" field that is checked here, but for protocol
        // files the name is just the name of the protocol, and for
        // non-protocols the name is derived from the file name, so this
        // checking should be equivalent.)
        let base_file_name = match tu.file_name.rsplit('/').next() { // FIXME: adhoc
            Some(fs) => fs,
            None => return Err(Errors::one(&Location { file_name: tu.file_name.clone(), lineno: 0, colno: 0 }, "File path has no file")),
        };
        let expected_file_name = ns.name.id.clone() + ".ipdl";
        if base_file_name != expected_file_name {
            return Err(Errors::one(&Location { file_name: tu.file_name.clone(), lineno: 0, colno: 0 }, &format!("expected file for translation unit `{}' to be named `{}'; instead it's named `{}'.",
                               tu.namespace.name.id, expected_file_name, base_file_name)))
        }
    }

    Ok(())
}


pub fn check(tus: &HashMap<TUId, TranslationUnit>) -> Result<(), Errors> {
    let mut tuts = HashMap::new();

    // XXX This ordering should be deterministic. I could sort by the
    // TUId.

    let tus_vec = tus.iter().collect::<Vec<_>>();

    for (&tuid, tu) in &tus_vec {
        try!(check_translation_unit(&tu));

        // Create top-level type decl for all protocols.
        let old_entry = tuts.insert(tuid, TranslationUnitType::new(&tu.protocol));
        assert!(old_entry.is_none());
    }

    for (&tuid, tu) in &tus_vec {
        try!(gather_decls_tu(&tus, &mut tuts, tuid, &tu));
    }

    let tuts_vec = tuts.iter().collect::<Vec<_>>();
    let mut defined = HashMap::new();
    for (&tuid, tut) in tuts_vec {
        try!(check_types_tu(&tus, &tuts, &mut defined, tuid, &tut));
    }

    Ok(())
}
