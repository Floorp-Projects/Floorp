use glsl_to_cxx::translate;
fn main() {
    println!("{}", translate(&mut std::env::args()));
}
